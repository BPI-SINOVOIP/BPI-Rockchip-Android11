/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.telephony4.cts;

import static androidx.test.InstrumentationRegistry.getContext;

import static org.junit.Assert.fail;

import android.content.Context;
import android.telephony.SmsManager;
import android.telephony.TelephonyManager;
import org.junit.Before;
import org.junit.Test;

public class SimRestrictedApisTest {
    private static final byte[] TEST_PDU = { 0, 0 };
    private TelephonyManager mTelephonyManager;

    @Before
    public void setUp() throws Exception {
        mTelephonyManager =
                (TelephonyManager) getContext().getSystemService(Context.TELEPHONY_SERVICE);
    }

    private boolean isSimCardPresent() {
        return mTelephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_NONE &&
                mTelephonyManager.getSimState() != TelephonyManager.SIM_STATE_ABSENT;
    }

    /**
     * Tests the SmsManager.injectSmsPdu() API. This makes a call to injectSmsPdu() API and expects
     * a SecurityException since the test apk is not signed by a certificate on the SIM.
     */
    @Test
    public void testInjectSmsPdu() {
        try {
            if (isSimCardPresent()) {
                SmsManager.getDefault().injectSmsPdu(TEST_PDU, "3gpp", null);
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.setLine1NumberForDisplay() API. This makes a call to
     * setLine1NumberForDisplay() API and expects a SecurityException since the test apk is not
     * signed by a certificate on the SIM.
     */
    @Test
    public void testSetLine1NumberForDisplay() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.setLine1NumberForDisplay("", "");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccOpenLogicalChannel() API. This makes a call to
     * iccOpenLogicalChannel() API and expects a SecurityException since the test apk is not signed
     * by certificate on the SIM.
     */
    @Test
    public void testIccOpenLogicalChannel() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccCloseLogicalChannel(
                        mTelephonyManager.iccOpenLogicalChannel("").getChannel());
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccOpenLogicalChannelBySlot() API. This makes a call to
     * iccOpenLogicalChannelBySlot() API and expects a SecurityException since the test apk is not
     * signed by certificate on the SIM.
     */
    @Test
    public void testIccOpenLogicalChannelBySlot() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccCloseLogicalChannelBySlot(0,
                        mTelephonyManager.iccOpenLogicalChannel("").getChannel());
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccCloseLogicalChannel() API. This makes a call to
     * iccCloseLogicalChannel() API and expects a SecurityException since the test apk is not signed
     * by certificate on the SIM.
     */
    @Test
    public void testIccCloseLogicalChannel() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccCloseLogicalChannel(0);
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccCloseLogicalChannelBySlot() API. This makes a call to
     * iccCloseLogicalChannelBySlot() API and expects a SecurityException since the test apk is not
     * signed by certificate on the SIM.
     */
    @Test
    public void testIccCloseLogicalChannelBySlot() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccCloseLogicalChannelBySlot(0, 0);
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccTransmitApduBasicChannel() API. This makes a call to
     * iccTransmitApduBasicChannel() API and expects a SecurityException since the test apk is not
     * signed by a certificate on the SIM.
     */
    @Test
    public void testIccTransmitApduBasicChannel() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccTransmitApduBasicChannel(0, 0, 0, 0, 0, "");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccTransmitApduBasicChannelBySlot() API. This makes a call to
     * iccTransmitApduBasicChannelBySlot() API and expects a SecurityException since the test apk is
     * not signed by a certificate on the SIM.
     */
    @Test
    public void testIccTransmitApduBasicChannelBySlot() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccTransmitApduBasicChannelBySlot(0, 0, 0, 0, 0, 0, "");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccTransmitApduLogicalChannel() API. This makes a call to
     * iccTransmitApduLogicalChannel() API and expects a SecurityException since the test apk is not
     * signed by a certificate on the SIM.
     */
    @Test
    public void testIccTransmitApduLogicalChannel() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccTransmitApduLogicalChannel(0, 0, 0, 0, 0, 0, "");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.iccTransmitApduLogicalChannelBySlot() API. This makes a call to
     * iccTransmitApduLogicalChannelBySlot() API and expects a SecurityException since the test apk
     * is not signed by a certificate on the SIM.
     */
    @Test
    public void testIccTransmitApduLogicalChannelBySlot() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.iccTransmitApduLogicalChannelBySlot(0, 0, 0, 0, 0, 0, 0, "");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.sendEnvelopeWithStatus() API. This makes a call to
     * sendEnvelopeWithStatus() API and expects a SecurityException since the test apk is not signed
     * by certificate on the SIM.
     */
    @Test
    public void testSendEnvelopeWithStatus() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.sendEnvelopeWithStatus("");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.nvReadItem() API. This makes a call to nvReadItem() API and
     * expects a SecurityException since the test apk is not signed by a certificate on the SIM.
     */
    @Test
    public void testNvReadItem() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.nvReadItem(0);
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.nvResetConfig() API. This makes a call to nvResetConfig() API and
     * expects a SecurityException since the test apk is not signed by a certificate on the SIM.
     */
    @Test
    public void testNvResetConfig() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.nvResetConfig(1);
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.getPreferredNetworkType() API. This makes a call to
     * getPreferredNetworkType() API and expects a SecurityException since the test apk is not
     * signed by certificate on the SIM.
     */
    @Test
    public void testGetPreferredNetworkType() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.getPreferredNetworkType(0);
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.setPreferredNetworkTypeToGlobal() API. This makes a call to
     * setPreferredNetworkTypeToGlobal() API and expects a SecurityException since the test apk is not
     * signed by certificate on the SIM.
     */
    @Test
    public void testSetPreferredNetworkTypeToGlobal() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.setPreferredNetworkTypeToGlobal();
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests that the test apk doesn't have carrier previliges.
     */
    @Test
    public void testHasCarrierPrivileges() {
        if (mTelephonyManager.hasCarrierPrivileges()) {
            fail("App unexpectedly has carrier privileges");
        }
    }

    /**
     * Tests the TelephonyManager.setOperatorBrandOverride() API. This makes a call to
     * setOperatorBrandOverride() API and expects a SecurityException since the test apk is not
     * signed by certificate on the SIM.
     */
    @Test
    public void testSetOperatorBrandOverride() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.setOperatorBrandOverride("");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.getIccAuthentication() API. This makes a call to
     * getIccAuthentication() API and expects a SecurityException since the test apk is not
     * signed by certificate on the SIM.
     */
    @Test
    public void testGetIccAuthentication() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA, "");
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests the TelephonyManager.getUiccCardsInfo() API. This makes a call to  getUiccCardsInfo()
     * API and expects a SecurityException since the test apk is not signed by certficate on the
     * SIM.
     */
    @Test
    public void testGetUiccCardsInfo() {
        try {
            if (isSimCardPresent()) {
                mTelephonyManager.getUiccCardsInfo();
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }
}
