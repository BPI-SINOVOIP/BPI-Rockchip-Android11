/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.carrierapi.cts;

import static android.carrierapi.cts.FcpTemplate.FILE_IDENTIFIER;
import static android.carrierapi.cts.IccUtils.bytesToHexString;
import static android.carrierapi.cts.IccUtils.hexStringToBytes;
import static android.telephony.IccOpenLogicalChannelResponse.INVALID_CHANNEL;
import static android.telephony.IccOpenLogicalChannelResponse.STATUS_NO_ERROR;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.BroadcastReceiver;
import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.ParcelUuid;
import android.os.PersistableBundle;
import android.provider.Telephony;
import android.provider.VoicemailContract;
import android.telephony.AvailableNetworkInfo;
import android.telephony.CarrierConfigManager;
import android.telephony.IccOpenLogicalChannelResponse;
import android.telephony.PhoneStateListener;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.Suppress;
import android.util.Base64;
import android.util.Log;

import com.android.compatibility.common.util.ShellIdentityUtils;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;
import java.util.stream.Collectors;

import javax.annotation.Nonnull;

// TODO(b/130187425): Split CarrierApiTest apart to have separate test classes for functionality
public class CarrierApiTest extends AndroidTestCase {
    private static final String TAG = "CarrierApiTest";
    private TelephonyManager mTelephonyManager;
    private CarrierConfigManager mCarrierConfigManager;
    private PackageManager mPackageManager;
    private SubscriptionManager mSubscriptionManager;
    private ContentProviderClient mVoicemailProvider;
    private ContentProviderClient mStatusProvider;
    private Uri mVoicemailContentUri;
    private Uri mStatusContentUri;
    private boolean hasCellular;
    private String selfPackageName;
    private String selfCertHash;
    private HandlerThread mListenerThread;

    private static final String FiDevCert = "24EB92CBB156B280FA4E1429A6ECEEB6E5C1BFE4";
    // The minimum allocatable logical channel number, per TS 102 221 Section 11.1.17.1
    private static final int MIN_LOGICAL_CHANNEL = 1;
    // The maximum allocatable logical channel number in the standard range, per TS 102 221 Section
    // 11.1.17.1
    private static final int MAX_LOGICAL_CHANNEL = 3;
    // Class bytes. The logical channel used should be included for bits b2b1. TS 102 221 Table 11.5
    private static final int CLA_GET_RESPONSE = 0x00;
    private static final int CLA_MANAGE_CHANNEL = 0x00;
    private static final int CLA_READ_BINARY = 0x00;
    private static final int CLA_SELECT = 0x00;
    private static final int CLA_STATUS = 0x80;
    private static final String CLA_STATUS_STRING = "80";
    // APDU Instruction Bytes. TS 102 221 Section 10.1.2
    private static final int COMMAND_GET_RESPONSE = 0xC0;
    private static final int COMMAND_MANAGE_CHANNEL = 0x70;
    private static final int COMMAND_READ_BINARY = 0xB0;
    private static final int COMMAND_SELECT = 0xA4;
    private static final int COMMAND_STATUS = 0xF2;
    private static final String COMMAND_STATUS_STRING = "F2";
    // Status words. TS 102 221 Section 10.2.1
    private static final byte[] STATUS_NORMAL = {(byte) 0x90, (byte) 0x00};
    private static final String STATUS_NORMAL_STRING = "9000";
    private static final String STATUS_BYTES_REMAINING = "61";
    private static final String STATUS_WARNING_A = "62";
    private static final String STATUS_WARNING_B = "63";
    private static final String STATUS_FILE_NOT_FOUND = "6a82";
    private static final String STATUS_INCORRECT_PARAMETERS = "6a86";
    private static final String STATUS_WRONG_PARAMETERS = "6b00";
    private static final Set<String> INVALID_PARAMETERS_STATUSES =
            new HashSet<>(Arrays.asList(STATUS_INCORRECT_PARAMETERS, STATUS_WRONG_PARAMETERS));
    private static final String STATUS_WRONG_CLASS = "6e00";
    // File ID for the EF ICCID. TS 102 221
    private static final String ICCID_FILE_ID = "2FE2";
    // File ID for the master file. TS 102 221
    private static final String MF_FILE_ID = "3F00";
    private static final int MF_FILE_ID_HEX = 0x3F00;
    // File ID for the MF Access Rule Reference. TS 102 221
    private static final String MF_ARR_FILE_ID = "2F06";
    private static final String ALPHA_TAG_A = "tagA";
    private static final String ALPHA_TAG_B = "tagB";
    private static final String NUMBER_A = "1234567890";
    private static final String NUMBER_B = "0987654321";
    private static final String TESTING_PLMN = "12345";

    private static final String EAP_SIM_AKA_RAND = "11111111111111111111111111111111";

    // Derived from TS 134 108#8.1.2. Based on EAP_SIM_AKA_RAND and assumed K value of
    // 000102030405060708090A0B0C0D0E0F, per TS 134 108#8.2
    private static final String EAP_AKA_AUTN = "12351417161900001130131215141716";

    // EAP-AKA Response Format: [DB][Length][RES][Length][CK][Length][IK]
    private static final int EAP_AKA_RESPONSE_LENGTH = 1 + 1 + 16 + 1 + 16 + 1 + 16;

    // Derived from TS 134 108#8.1.2. Based on EAP_SIM_AKA_RAND and assumed K value of
    // 000102030405060708090A0B0C0D0E0F, per TS 134 108#8.2.
    // Format: [DB][Length][RES][Length][CK][Length][IK]
    private static final String EXPECTED_EAP_AKA_RESULT =
            "DB10111013121514171619181B1A1D1C1F1E"
                    + "101013121514171619181B1A1D1C1F1E11"
                    + "1013121514171619181B1A1D1C1F1E1110";

    // Derived from TS 134 108#8.1.2 and TS 133 102#6.8.1.2. Based on EAP_SIM_AKA_RAND and assumed K
    // value of 000102030405060708090A0B0C0D0E0F, per TS 134 108#8.2.
    // Format: [Length][SRES][Length][Kc]
    private static final String EXPECTED_EAP_SIM_RESULT = "0400000000080000000000000000";

    private static final int DSDS_PHONE_COUNT = 2;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mPackageManager = getContext().getPackageManager();
        mTelephonyManager = (TelephonyManager)
                getContext().getSystemService(Context.TELEPHONY_SERVICE);
        hasCellular = hasCellular();
        if (!hasCellular) {
            Log.e(TAG, "No cellular support, all tests will be skipped.");
            return;
        }

        mCarrierConfigManager = (CarrierConfigManager)
                getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        mSubscriptionManager = (SubscriptionManager)
                getContext().getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        selfPackageName = getContext().getPackageName();
        selfCertHash = getCertHash(selfPackageName);
        mVoicemailContentUri = VoicemailContract.Voicemails.buildSourceUri(selfPackageName);
        mVoicemailProvider = getContext().getContentResolver()
                .acquireContentProviderClient(mVoicemailContentUri);
        mStatusContentUri = VoicemailContract.Status.buildSourceUri(selfPackageName);
        mStatusProvider = getContext().getContentResolver()
                .acquireContentProviderClient(mStatusContentUri);
        mListenerThread = new HandlerThread("CarrierApiTest");
        mListenerThread.start();
    }

    @Override
    public void tearDown() throws Exception {
        if (!hasCellular) return;

        mListenerThread.quit();
        try {
            mStatusProvider.delete(mStatusContentUri, null, null);
            mVoicemailProvider.delete(mVoicemailContentUri, null, null);
        } catch (Exception e) {
            Log.w(TAG, "Failed to clean up voicemail tables in tearDown", e);
        }
        super.tearDown();
    }

    /**
     * Checks whether the cellular stack should be running on this device.
     */
    private boolean hasCellular() {
        return mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY) &&
                mTelephonyManager.getPhoneCount() > 0;
    }

    private boolean isSimCardPresent() {
        return mTelephonyManager.getSimState() != TelephonyManager.SIM_STATE_ABSENT;
    }

    private String getCertHash(String pkgName) {
        try {
            PackageInfo pInfo = mPackageManager.getPackageInfo(pkgName,
                    PackageManager.GET_SIGNATURES | PackageManager.GET_DISABLED_UNTIL_USED_COMPONENTS);
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            return bytesToHexString(md.digest(pInfo.signatures[0].toByteArray()));
        } catch (PackageManager.NameNotFoundException ex) {
            Log.e(TAG, pkgName + " not found", ex);
        } catch (NoSuchAlgorithmException ex) {
            Log.e(TAG, "Algorithm SHA1 is not found.");
        }
        return "";
    }

    private void failMessage() {
        if (FiDevCert.equalsIgnoreCase(selfCertHash)) {
            fail("This test requires a Project Fi SIM card.");
        } else {
            fail("This test requires a SIM card with carrier privilege rule on it.\n" +
                 "Cert hash: " + selfCertHash + "\n" +
                 "Visit https://source.android.com/devices/tech/config/uicc.html");
        }
    }

    public void testSimCardPresent() {
        if (!hasCellular) return;
        assertTrue("This test requires SIM card.", isSimCardPresent());
    }

    public void testHasCarrierPrivileges() {
        if (!hasCellular) return;
        if (!mTelephonyManager.hasCarrierPrivileges()) {
            failMessage();
        }
    }

    private static void assertUpdateAvailableNetworkSuccess(int value) {
        assertEquals(TelephonyManager.UPDATE_AVAILABLE_NETWORKS_SUCCESS, value);
    }

    private static void assertUpdateAvailableNetworkNoOpportunisticSubAvailable(int value) {
        assertEquals(
                TelephonyManager.UPDATE_AVAILABLE_NETWORKS_NO_OPPORTUNISTIC_SUB_AVAILABLE, value);
    }

    private static void assertSetOpportunisticSubSuccess(int value) {
        assertEquals(TelephonyManager.SET_OPPORTUNISTIC_SUB_SUCCESS, value);
    }

    private int getFirstActivateCarrierPrivilegedSubscriptionId() {
        int subId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        List<SubscriptionInfo> subscriptionInfos =
                mSubscriptionManager.getActiveSubscriptionInfoList();
        if (subscriptionInfos != null) {
            for (SubscriptionInfo info : subscriptionInfos) {
                TelephonyManager telephonyManager = mTelephonyManager.createForSubscriptionId(
                        info.getSubscriptionId());
                if (telephonyManager.hasCarrierPrivileges()) {
                    subId = info.getSubscriptionId();
                    return subId;
                }
            }
        }
        return subId;
    }

    public void testUpdateAvailableNetworksWithCarrierPrivilege() {
        if (!hasCellular) return;

        int subIdWithCarrierPrivilege = getFirstActivateCarrierPrivilegedSubscriptionId();
        int activeSubscriptionInfoCount = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mSubscriptionManager, (tm) -> tm.getActiveSubscriptionInfoCount());
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        if (mTelephonyManager.getPhoneCount() == 1) {
            return;
        }

        /* TODO: b/145993690 */
        if (mTelephonyManager.getPhoneCount() == 2 && activeSubscriptionInfoCount != 2) {
            /* This test requires two SIM cards */
            return;
        }
        if (subIdWithCarrierPrivilege == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            /* This test requires SIM with carrier privilege */
            return;
        }

        List<SubscriptionInfo> subscriptionInfoList =
                mSubscriptionManager.getOpportunisticSubscriptions();
        List<String> mccMncs = new ArrayList<String>();
        List<Integer> bands = new ArrayList<Integer>();
        List<AvailableNetworkInfo> availableNetworkInfos = new ArrayList<AvailableNetworkInfo>();
        Consumer<Integer> callbackSuccess =
                CarrierApiTest::assertUpdateAvailableNetworkSuccess;
        Consumer<Integer> callbackNoOpportunisticSubAvailable =
                CarrierApiTest::assertUpdateAvailableNetworkNoOpportunisticSubAvailable;
        Consumer<Integer> setOpCallbackSuccess = CarrierApiTest::assertSetOpportunisticSubSuccess;
        if (subscriptionInfoList == null || subscriptionInfoList.size() == 0
                || !mSubscriptionManager.isActiveSubscriptionId(
                        subscriptionInfoList.get(0).getSubscriptionId())) {
            try {
                AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(
                        subIdWithCarrierPrivilege, AvailableNetworkInfo.PRIORITY_HIGH, mccMncs,
                        bands);
                availableNetworkInfos.add(availableNetworkInfo);
                // Call updateAvailableNetworks without opportunistic subscription.
                // callbackNoOpportunisticSubAvailable is expected to be triggered
                // and the return value will be checked against
                // UPDATE_AVAILABLE_NETWORKS_NO_OPPORTUNISTIC_SUB_AVAILABLE
                mTelephonyManager.updateAvailableNetworks(availableNetworkInfos,
                        AsyncTask.SERIAL_EXECUTOR, callbackNoOpportunisticSubAvailable);
            } finally {
                // clear all the operations at the end of test.
                availableNetworkInfos.clear();
                mTelephonyManager.updateAvailableNetworks(availableNetworkInfos,
                        AsyncTask.SERIAL_EXECUTOR, callbackNoOpportunisticSubAvailable);
            }
        } else {
            // This is case of DSDS phone, one active opportunistic subscription and one
            // active primary subscription.
            int resultSubId;
            try {
                AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(
                        subscriptionInfoList.get(0).getSubscriptionId(),
                        AvailableNetworkInfo.PRIORITY_HIGH, mccMncs, bands);
                availableNetworkInfos.add(availableNetworkInfo);
                mTelephonyManager.updateAvailableNetworks(availableNetworkInfos,
                        AsyncTask.SERIAL_EXECUTOR, callbackSuccess);
                // wait for the data change to take effect
                waitForMs(500);
                // Call setPreferredData and reconfirm with getPreferred data
                // that the same is updated.
                int preferSubId = subscriptionInfoList.get(0).getSubscriptionId();
                mTelephonyManager.setPreferredOpportunisticDataSubscription(
                        preferSubId, false, AsyncTask.SERIAL_EXECUTOR, setOpCallbackSuccess);
                // wait for the data change to take effect
                waitForMs(500);
                resultSubId = mTelephonyManager.getPreferredOpportunisticDataSubscription();
                assertEquals(preferSubId, resultSubId);
            } finally {
                // clear all the operations at the end of test.
                availableNetworkInfos.clear();
                mTelephonyManager.updateAvailableNetworks(availableNetworkInfos,
                        AsyncTask.SERIAL_EXECUTOR, callbackSuccess);
                waitForMs(500);
                mTelephonyManager.setPreferredOpportunisticDataSubscription(
                        SubscriptionManager.DEFAULT_SUBSCRIPTION_ID, false,
                        AsyncTask.SERIAL_EXECUTOR, callbackSuccess);
                waitForMs(500);
                resultSubId = mTelephonyManager.getPreferredOpportunisticDataSubscription();
                assertEquals(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID, resultSubId);
            }
        }
    }

    public static void waitForMs(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Log.d(TAG, "InterruptedException while waiting: " + e);
        }
    }

    public void testGetIccAuthentication() {
        // EAP-SIM rand is 16 bytes.
        String base64Challenge = "ECcTqwuo6OfY8ddFRboD9WM=";
        String base64Challenge2 = "EMNxjsFrPCpm+KcgCmQGnwQ=";
        if (!hasCellular) return;
        try {
            assertNull("getIccAuthentication should return null for empty data.",
                    mTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                    TelephonyManager.AUTHTYPE_EAP_AKA, ""));
            String response = mTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                    TelephonyManager.AUTHTYPE_EAP_SIM, base64Challenge);
            assertTrue("Response to EAP-SIM Challenge must not be Null.", response != null);
            // response is base64 encoded. After decoding, the value should be:
            // 1 length byte + SRES(4 bytes) + 1 length byte + Kc(8 bytes)
            byte[] result = android.util.Base64.decode(response, android.util.Base64.DEFAULT);
            assertTrue("Result length must be 14 bytes.", 14 == result.length);
            String response2 = mTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                    TelephonyManager.AUTHTYPE_EAP_SIM, base64Challenge2);
            assertTrue("Two responses must be different.", !response.equals(response2));
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testSendDialerSpecialCode() {
        if (!hasCellular) return;
        try {
            IntentReceiver intentReceiver = new IntentReceiver();
            final IntentFilter intentFilter = new IntentFilter();
            intentFilter.addAction(Telephony.Sms.Intents.SECRET_CODE_ACTION);
            intentFilter.addDataScheme("android_secret_code");
            getContext().registerReceiver(intentReceiver, intentFilter);

            mTelephonyManager.sendDialerSpecialCode("4636");
            assertTrue("Did not receive expected Intent: " +
                    Telephony.Sms.Intents.SECRET_CODE_ACTION,
                    intentReceiver.waitForReceive());
        } catch (SecurityException e) {
            failMessage();
        } catch (InterruptedException e) {
            Log.d(TAG, "Broadcast receiver wait was interrupted.");
        }
    }

    public void testSubscriptionInfoListing() {
        if (!hasCellular) return;
        try {
            assertTrue("getActiveSubscriptionInfoCount() should be non-zero",
                    mSubscriptionManager.getActiveSubscriptionInfoCount() > 0);
            List<SubscriptionInfo> subInfoList =
                    mSubscriptionManager.getActiveSubscriptionInfoList();
            assertNotNull("getActiveSubscriptionInfoList() returned null", subInfoList);
            assertFalse("getActiveSubscriptionInfoList() returned an empty list",
                    subInfoList.isEmpty());
            for (SubscriptionInfo info : subInfoList) {
                TelephonyManager tm =
                        mTelephonyManager.createForSubscriptionId(info.getSubscriptionId());
                assertTrue("getActiveSubscriptionInfoList() returned an inaccessible subscription",
                        tm.hasCarrierPrivileges());

                // Check other APIs to make sure they are accessible and return consistent info.
                SubscriptionInfo infoForSlot =
                        mSubscriptionManager.getActiveSubscriptionInfoForSimSlotIndex(
                                info.getSimSlotIndex());
                assertNotNull("getActiveSubscriptionInfoForSimSlotIndex() returned null",
                        infoForSlot);
                assertEquals(
                        "getActiveSubscriptionInfoForSimSlotIndex() returned inconsistent info",
                        info.getSubscriptionId(), infoForSlot.getSubscriptionId());

                SubscriptionInfo infoForSubId =
                        mSubscriptionManager.getActiveSubscriptionInfo(info.getSubscriptionId());
                assertNotNull("getActiveSubscriptionInfo() returned null", infoForSubId);
                assertEquals("getActiveSubscriptionInfo() returned inconsistent info",
                        info.getSubscriptionId(), infoForSubId.getSubscriptionId());
            }
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testCarrierConfigIsAccessible() {
        if (!hasCellular) return;
        try {
            PersistableBundle bundle = mCarrierConfigManager.getConfig();
            assertNotNull("CarrierConfigManager#getConfig() returned null", bundle);
            assertFalse("CarrierConfigManager#getConfig() returned empty bundle", bundle.isEmpty());

            int subId = SubscriptionManager.getDefaultSubscriptionId();
            bundle = mCarrierConfigManager.getConfigForSubId(subId);
            assertNotNull("CarrierConfigManager#getConfigForSubId() returned null", bundle);
            assertFalse("CarrierConfigManager#getConfigForSubId() returned empty bundle",
                    bundle.isEmpty());
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testTelephonyApisAreAccessible() {
        if (!hasCellular) return;
        // The following methods may return any value depending on the state of the device. Simply
        // call them to make sure they do not throw any exceptions. Methods that return a device
        // identifier will be accessible to apps with carrier privileges in Q, but this may change
        // in a future release.
        try {
            mTelephonyManager.getDeviceId();
            mTelephonyManager.getImei();
            mTelephonyManager.getMeid();
            mTelephonyManager.getDeviceSoftwareVersion();
            mTelephonyManager.getNai();
            mTelephonyManager.getDataNetworkType();
            mTelephonyManager.getVoiceNetworkType();
            mTelephonyManager.getSimSerialNumber();
            mTelephonyManager.getSubscriberId();
            mTelephonyManager.getGroupIdLevel1();
            mTelephonyManager.getLine1Number();
            mTelephonyManager.getVoiceMailNumber();
            mTelephonyManager.getVisualVoicemailPackageName();
            mTelephonyManager.getVoiceMailAlphaTag();
            mTelephonyManager.getForbiddenPlmns();
            mTelephonyManager.getServiceState();
            mTelephonyManager.getManualNetworkSelectionPlmn();
            mTelephonyManager.setForbiddenPlmns(new ArrayList<String>());
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testVoicemailTableIsAccessible() throws Exception {
        if (!hasCellular) return;
        ContentValues value = new ContentValues();
        value.put(VoicemailContract.Voicemails.NUMBER, "0123456789");
        value.put(VoicemailContract.Voicemails.SOURCE_PACKAGE, selfPackageName);
        try {
            Uri uri = mVoicemailProvider.insert(mVoicemailContentUri, value);
            assertNotNull(uri);
            Cursor cursor = mVoicemailProvider.query(uri,
                    new String[] {
                            VoicemailContract.Voicemails.NUMBER,
                            VoicemailContract.Voicemails.SOURCE_PACKAGE
                    }, null, null, null);
            assertNotNull(cursor);
            assertTrue(cursor.moveToFirst());
            assertEquals("0123456789", cursor.getString(0));
            assertEquals(selfPackageName, cursor.getString(1));
            assertFalse(cursor.moveToNext());
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testVoicemailStatusTableIsAccessible() throws Exception {
        if (!hasCellular) return;
        ContentValues value = new ContentValues();
        value.put(VoicemailContract.Status.CONFIGURATION_STATE,
                VoicemailContract.Status.CONFIGURATION_STATE_OK);
        value.put(VoicemailContract.Status.SOURCE_PACKAGE, selfPackageName);
        try {
            Uri uri = mStatusProvider.insert(mStatusContentUri, value);
            assertNotNull(uri);
            Cursor cursor = mVoicemailProvider.query(uri,
                    new String[] {
                            VoicemailContract.Status.CONFIGURATION_STATE,
                            VoicemailContract.Status.SOURCE_PACKAGE
                    }, null, null, null);
            assertNotNull(cursor);
            assertTrue(cursor.moveToFirst());
            assertEquals(VoicemailContract.Status.CONFIGURATION_STATE_OK, cursor.getInt(0));
            assertEquals(selfPackageName, cursor.getString(1));
            assertFalse(cursor.moveToNext());
        } catch (SecurityException e) {
            failMessage();
        }
    }

    static final int READ_PHONE_STATE_LISTENERS =
            PhoneStateListener.LISTEN_CALL_FORWARDING_INDICATOR
                    | PhoneStateListener.LISTEN_MESSAGE_WAITING_INDICATOR
                    | PhoneStateListener.LISTEN_EMERGENCY_NUMBER_LIST;

    static final int READ_PRECISE_PHONE_STATE_LISTENERS =
            PhoneStateListener.LISTEN_PRECISE_DATA_CONNECTION_STATE
                    | PhoneStateListener.LISTEN_CALL_DISCONNECT_CAUSES
                    | PhoneStateListener.LISTEN_IMS_CALL_DISCONNECT_CAUSES
                    | PhoneStateListener.LISTEN_REGISTRATION_FAILURE
                    | PhoneStateListener.LISTEN_BARRING_INFO;

    static final int CARRIER_PRIVILEGE_LISTENERS =
            READ_PHONE_STATE_LISTENERS | READ_PRECISE_PHONE_STATE_LISTENERS;

    public void testGetManualNetworkSelectionPlmnPersisted() throws Exception {
        if (!hasCellular) return;
        if (mTelephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_GSM) return;

        try {
            mTelephonyManager.setNetworkSelectionModeManual(
                     TESTING_PLMN/* operatorNumeric */, true /* persistSelection */);
            String plmn = mTelephonyManager.getManualNetworkSelectionPlmn();
            assertEquals(TESTING_PLMN, plmn);
        } finally {
            mTelephonyManager.setNetworkSelectionModeAutomatic();
        }
    }

    public void testPhoneStateListener() throws Exception {
        if (!hasCellular) return;
        PhoneStateListener psl = new PhoneStateListener((Runnable r) -> { });
        try {
            mTelephonyManager.listen(psl, CARRIER_PRIVILEGE_LISTENERS);
        } finally {
            mTelephonyManager.listen(psl, PhoneStateListener.LISTEN_NONE);
        }
    }

    public void testIsManualNetworkSelectionAllowed() throws Exception {
        if (!hasCellular) return;
        if (mTelephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_GSM) return;

        try {
            assertTrue(mTelephonyManager.isManualNetworkSelectionAllowed());
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testGetNetworkSelectionMode() throws Exception {
        if (!hasCellular) return;

        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.setNetworkSelectionModeAutomatic());
            int networkMode = mTelephonyManager.getNetworkSelectionMode();
            assertEquals(TelephonyManager.NETWORK_SELECTION_MODE_AUTO, networkMode);
        } catch (SecurityException e) {
            failMessage();
        }
    }

    public void testSubscriptionInfoChangeListener() throws Exception {
        if (!hasCellular) return;
        final AtomicReference<SecurityException> error = new AtomicReference<>();
        final CountDownLatch latch = new CountDownLatch(1);
        new Handler(mListenerThread.getLooper()).post(() -> {
            SubscriptionManager.OnSubscriptionsChangedListener listener =
                    new SubscriptionManager.OnSubscriptionsChangedListener();
            try {
                mSubscriptionManager.addOnSubscriptionsChangedListener(listener);
            } catch (SecurityException e) {
                error.set(e);
            } finally {
                mSubscriptionManager.removeOnSubscriptionsChangedListener(listener);
                latch.countDown();
            }
        });
        assertTrue("Test timed out", latch.await(30L, TimeUnit.SECONDS));
        if (error.get() != null) {
            failMessage();
        }

    }

    /**
     * Test that it's possible to open logical channels to the ICC. This mirrors the Manage Channel
     * command described in TS 102 221 Section 11.1.17.
     */
    public void testIccOpenLogicalChannel() {
        if (!hasCellular) return;

        // The AID here doesn't matter - we just need to open a valid connection. In this case, the
        // specified AID ("") opens a channel and selects the MF.
        IccOpenLogicalChannelResponse response = mTelephonyManager.iccOpenLogicalChannel("");
        final int logicalChannel = response.getChannel();
        try {
            verifyValidIccOpenLogicalChannelResponse(response);
        } finally {
            mTelephonyManager.iccCloseLogicalChannel(logicalChannel);
        }
    }

    public void testIccOpenLogicalChannelWithValidP2() {
        if (!hasCellular) return;

        // {@link TelephonyManager#iccOpenLogicalChannel} sends a Manage Channel (open) APDU
        // followed by a Select APDU with the given AID and p2 values. See Open Mobile API
        // Specification v3.2 Section 6.2.7.h and TS 102 221 for details.
        int p2 = 0x0C; // '0C' for no data returned (TS 102 221 Section 11.1.1.2)
        IccOpenLogicalChannelResponse response = mTelephonyManager.iccOpenLogicalChannel("", p2);
        final int logicalChannel = response.getChannel();
        try {
            verifyValidIccOpenLogicalChannelResponse(response);
        } finally {
            mTelephonyManager.iccCloseLogicalChannel(logicalChannel);
        }
    }

    public void testIccOpenLogicalChannelWithInvalidP2() {
        if (!hasCellular) return;

        // Valid p2 values are defined in TS 102 221 Table 11.2. Per Table 11.2, 0xF0 should be
        // invalid. Any p2 values that produce non '9000'/'62xx'/'63xx' status words are treated as
        // an error and the channel is not opened. Due to compatibility issues with older devices,
        // this check is only enabled for new devices launching on Q+.
        if (Build.VERSION.FIRST_SDK_INT >= Build.VERSION_CODES.Q) {
            int p2 = 0xF0;
            IccOpenLogicalChannelResponse response =
                    mTelephonyManager.iccOpenLogicalChannel("", p2);
            final int logicalChannel = response.getChannel();
            assertEquals(INVALID_CHANNEL, logicalChannel);
            assertNotEquals(STATUS_NO_ERROR, response.getStatus());
        }
    }

    /**
     * Test that it's possible to close logical channels to the ICC. This follows the Manage Channel
     * command described in TS 102 221 Section 11.1.17.
     */
    public void testIccCloseLogicalChannel() {
        if (!hasCellular) return;

        // The directory here doesn't matter - we just need to open a valid connection that can
        // later be closed. In this case, the specified AID ("") opens a channel and selects the MF.
        IccOpenLogicalChannelResponse response = mTelephonyManager.iccOpenLogicalChannel("");

        // Check that the select command succeeded. This ensures that the logical channel is indeed
        // open.
        assertArrayEquals(STATUS_NORMAL, response.getSelectResponse());
        assertTrue(mTelephonyManager.iccCloseLogicalChannel(response.getChannel()));

        // Close opened channel twice.
        assertFalse(mTelephonyManager.iccCloseLogicalChannel(response.getChannel()));

        // Channel 0 is guaranteed to be always available and cannot be closed, per TS 102 221
        // Section 11.1.17
        assertFalse(mTelephonyManager.iccCloseLogicalChannel(0));
    }

    /**
     * This test ensures that valid APDU instructions can be sent and processed by the ICC. To do
     * so, APDUs are sent to:
     * - get the status of the MF
     * - select the Access Rule Reference (ARR) for the MF
     * - get the FCP template response for the select
     */
    public void testIccTransmitApduLogicalChannel() {
        if (!hasCellular) return;

        // An open LC is required for transmitting APDU commands. This opens an LC to the MF.
        IccOpenLogicalChannelResponse iccOpenLogicalChannelResponse =
                mTelephonyManager.iccOpenLogicalChannel("");

        // Get the status of the current directory. This should match the MF. TS 102 221 Section
        // 11.1.2
        final int logicalChannel = iccOpenLogicalChannelResponse.getChannel();

        try {
            int cla = CLA_STATUS;
            int p1 = 0; // no indication of application status
            int p2 = 0; // same response parameters as the SELECT in the iccOpenLogicalChannel()
                        // above
            int p3 = 0; // length of 'data' payload
            String data = "";
            String response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_STATUS, p1, p2, p3, data);
            FcpTemplate fcpTemplate = FcpTemplate.parseFcpTemplate(response);
            // Check that the FCP Template's file ID matches the MF
            assertTrue(containsFileId(fcpTemplate, MF_FILE_ID));
            assertEquals(STATUS_NORMAL_STRING, fcpTemplate.getStatus());

            // Select the Access Rule Reference for the MF. Similar to the MF, this will exist
            // across all SIM cards. TS 102 221 Section 11.1.1
            cla = CLA_SELECT;
            p1 = 0; // select EF by FID
            p2 = 0x04; // requesting FCP template
            p3 = 2; // data (FID to be selected) is 2 bytes
            data = MF_ARR_FILE_ID;
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_SELECT, p1, p2, p3, data);

            // Devices launching with Q or later must immediately return the FCP template from the
            // previous SELECT command. Some devices that launched before Q return TPDUs (instead of
            // APDUs) - these devices must issue a subsequent GET RESPONSE command to get the FCP
            // template.
            if (Build.VERSION.FIRST_SDK_INT < Build.VERSION_CODES.Q) {
                // Conditionally need to send GET RESPONSE apdu based on response from
                // TelephonyManager
                if (response.startsWith(STATUS_BYTES_REMAINING)) {
                    // Read the FCP template from the ICC. TS 102 221 Section 12.1.1
                    cla = CLA_GET_RESPONSE;
                    p1 = 0;
                    p2 = 0;
                    p3 = 0;
                    data = "";
                    response =
                            mTelephonyManager.iccTransmitApduLogicalChannel(
                                    logicalChannel, cla, COMMAND_GET_RESPONSE, p1, p2, p3, data);
                }
            }

            fcpTemplate = FcpTemplate.parseFcpTemplate(response);
            // Check that the FCP Template's file ID matches the selected ARR
            assertTrue(containsFileId(fcpTemplate, MF_ARR_FILE_ID));
            assertEquals(STATUS_NORMAL_STRING, fcpTemplate.getStatus());
        } finally {
            mTelephonyManager.iccCloseLogicalChannel(logicalChannel);
        }
    }

    /**
     * Tests several invalid APDU instructions over a logical channel and makes sure appropriate
     * errors are returned from the UICC.
     */
    public void testIccTransmitApduLogicalChannelWithInvalidInputs() {
        if (!hasCellular) return;

        // An open LC is required for transmitting apdu commands. This opens an LC to the MF.
        IccOpenLogicalChannelResponse iccOpenLogicalChannelResponse =
                mTelephonyManager.iccOpenLogicalChannel("");
        final int logicalChannel = iccOpenLogicalChannelResponse.getChannel();

        try {
            // Make some invalid APDU commands and make sure they fail as expected.
            // Use an invalid p1 value for Status apdu
            int cla = CLA_STATUS | logicalChannel;
            int p1 = 0xFF; // only '00', '01', and '02' are allowed
            int p2 = 0; // same response parameters as the SELECT in the iccOpenLogicalChannel()
                        // above
            int p3 = 0; // length of 'data' payload
            String data = "";
            String response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_STATUS, p1, p2, p3, data);
            assertTrue(INVALID_PARAMETERS_STATUSES.contains(response));

            // Select a file that doesn't exist
            cla = CLA_SELECT;
            p1 = 0x00; // select by file ID
            p2 = 0x0C; // no data returned
            p3 = 0x02; // length of 'data' payload
            data = "FFFF"; // invalid file ID
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_SELECT, p1, p2, p3, data);
            assertEquals(STATUS_FILE_NOT_FOUND, response);

            // Manage channel with incorrect p1 parameter
            cla = CLA_MANAGE_CHANNEL | logicalChannel;
            p1 = 0x83; // Only '80' or '00' allowed for Manage Channel p1
            p2 = logicalChannel; // channel to be closed
            p3 = 0; // length of 'data' payload
            data = "";
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_MANAGE_CHANNEL, p1, p2, p3, data);
            assertTrue(isErrorResponse(response));

            // Use an incorrect class byte for Status apdu
            cla = 0xFF;
            p1 = 0; // no indication of application status
            p2 = 0; // same response parameters as the SELECT in the iccOpenLogicalChannel() above
            p3 = 0; // length of 'data' payload
            data = "";
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_STATUS, p1, p2, p3, data);
            assertEquals(STATUS_WRONG_CLASS, response);

            // Provide a data field that is longer than described for Select apdu
            cla = CLA_SELECT | logicalChannel;
            p1 = 0; // select by file ID
            p2 = 0x0C; // no data returned
            p3 = 0x04; // data passed is actually 2 bytes long
            data = "3F00"; // valid ID
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, COMMAND_SELECT, p1, p2, p3, data);
            assertTrue(isErrorResponse(response));

            // Use an invalid instruction
            cla = 0;
            p1 = 0;
            p2 = 0;
            p3 = 0;
            data = "";
            int invalidInstruction = 0xFF; // see TS 102 221 Table 10.5 for valid instructions
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, cla, invalidInstruction, p1, p2, p3, data);
            assertTrue(isErrorResponse(response));
        } finally {
            mTelephonyManager.iccCloseLogicalChannel(logicalChannel);
        }
    }

    /**
     * This test ensures that files can be read off the UICC. This helps to test the SIM booting
     * process, as it process involves several file-reads. The ICCID is one of the first files read.
     */
    public void testApduFileRead() {
        if (!hasCellular) return;

        // Open a logical channel and select the MF.
        IccOpenLogicalChannelResponse iccOpenLogicalChannel =
                mTelephonyManager.iccOpenLogicalChannel("");
        final int logicalChannel = iccOpenLogicalChannel.getChannel();

        try {
            // Select the ICCID. TS 102 221 Section 13.2
            int p1 = 0; // select by file ID
            int p2 = 0x0C; // no data returned
            int p3 = 2; // length of 'data' payload
            String response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, CLA_SELECT, COMMAND_SELECT, p1, p2, p3, ICCID_FILE_ID);
            assertEquals(STATUS_NORMAL_STRING, response);

            // Read the contents of the ICCID.
            p1 = 0; // 0-byte offset
            p2 = 0; // 0-byte offset
            p3 = 0; // length of 'data' payload
            response =
                    mTelephonyManager.iccTransmitApduLogicalChannel(
                            logicalChannel, CLA_READ_BINARY, COMMAND_READ_BINARY, p1, p2, p3, "");
            assertTrue(response.endsWith(STATUS_NORMAL_STRING));
        } finally {
            mTelephonyManager.iccCloseLogicalChannel(logicalChannel);
        }
    }

    /**
     * This test sends several valid APDU commands over the basic channel (channel 0).
     */
    public void testIccTransmitApduBasicChannel() {
        if (!hasCellular) return;

        // select the MF
        int cla = CLA_SELECT;
        int p1 = 0; // select EF by FID
        int p2 = 0x0C; // requesting FCP template
        int p3 = 2; // length of 'data' payload
        String data = MF_FILE_ID;
        String response = mTelephonyManager
            .iccTransmitApduBasicChannel(cla, COMMAND_SELECT, p1, p2, p3, data);
        assertEquals(STATUS_NORMAL_STRING, response);

        // get the Status of the current file/directory
        cla = CLA_STATUS;
        p1 = 0; // no indication of application status
        p2 = 0; // same response parameters as the SELECT in the iccOpenLogicalChannel() above
        p3 = 0; // length of 'data' payload
        data = "";
        response = mTelephonyManager
            .iccTransmitApduBasicChannel(cla, COMMAND_STATUS, p1, p2, p3, data);
        FcpTemplate fcpTemplate = FcpTemplate.parseFcpTemplate(response);
        assertTrue(containsFileId(fcpTemplate, MF_FILE_ID));

        // Manually open a logical channel
        cla = CLA_MANAGE_CHANNEL;
        p1 = 0; // open a logical channel
        p2 = 0; // '00' for open command
        p3 = 0; // length of data payload
        data = "";
        response = mTelephonyManager
            .iccTransmitApduBasicChannel(cla, COMMAND_MANAGE_CHANNEL, p1, p2, p3, data);
        // response is in the format | 1 byte: channel number | 2 bytes: status word |
        String responseStatus = response.substring(2);
        assertEquals(STATUS_NORMAL_STRING, responseStatus);

        // Close the open channel
        byte[] responseBytes = hexStringToBytes(response);
        int channel = responseBytes[0];
        cla = CLA_MANAGE_CHANNEL;
        p1 = 0x80; // close a logical channel
        p2 = channel; // the channel to be closed
        p3 = 0; // length of data payload
        data = "";
        response = mTelephonyManager
            .iccTransmitApduBasicChannel(cla, COMMAND_MANAGE_CHANNEL, p1, p2, p3, data);
        assertEquals(STATUS_NORMAL_STRING, response);
    }

    /**
     * This test verifies that {@link TelephonyManager#setLine1NumberForDisplay(String, String)}
     * correctly sets the Line 1 alpha tag and number when called.
     */
    public void testLine1NumberForDisplay() {
        if (!hasCellular) return;

        // Cache original alpha tag and number values.
        String originalAlphaTag = mTelephonyManager.getLine1AlphaTag();
        String originalNumber = mTelephonyManager.getLine1Number();

        try {
            // clear any potentially overridden values and cache defaults
            mTelephonyManager.setLine1NumberForDisplay(null, null);
            String defaultAlphaTag = mTelephonyManager.getLine1AlphaTag();
            String defaultNumber = mTelephonyManager.getLine1Number();

            assertTrue(mTelephonyManager.setLine1NumberForDisplay(ALPHA_TAG_A, NUMBER_A));
            assertEquals(ALPHA_TAG_A, mTelephonyManager.getLine1AlphaTag());
            assertEquals(NUMBER_A, mTelephonyManager.getLine1Number());

            assertTrue(mTelephonyManager.setLine1NumberForDisplay(ALPHA_TAG_B, NUMBER_B));
            assertEquals(ALPHA_TAG_B, mTelephonyManager.getLine1AlphaTag());
            assertEquals(NUMBER_B, mTelephonyManager.getLine1Number());

            // null is used to clear the Line 1 alpha tag and number values.
            assertTrue(mTelephonyManager.setLine1NumberForDisplay(null, null));
            assertEquals(defaultAlphaTag, mTelephonyManager.getLine1AlphaTag());
            assertEquals(defaultNumber, mTelephonyManager.getLine1Number());
        } finally {
            // Reset original alpha tag and number values.
            mTelephonyManager.setLine1NumberForDisplay(originalAlphaTag, originalNumber);
        }
    }

    /**
     * This test verifies that {@link TelephonyManager#setVoiceMailNumber(String, String)} correctly
     * sets the VoiceMail alpha tag and number when called.
     */
    public void testVoiceMailNumber() {
        if (!hasCellular) return;

        // Cache original alpha tag and number values.
        String originalAlphaTag = mTelephonyManager.getVoiceMailAlphaTag();
        String originalNumber = mTelephonyManager.getVoiceMailNumber();

        try {
            assertTrue(mTelephonyManager.setVoiceMailNumber(ALPHA_TAG_A, NUMBER_A));
            assertEquals(ALPHA_TAG_A, mTelephonyManager.getVoiceMailAlphaTag());
            assertEquals(NUMBER_A, mTelephonyManager.getVoiceMailNumber());

            assertTrue(mTelephonyManager.setVoiceMailNumber(ALPHA_TAG_B, NUMBER_B));
            assertEquals(ALPHA_TAG_B, mTelephonyManager.getVoiceMailAlphaTag());
            assertEquals(NUMBER_B, mTelephonyManager.getVoiceMailNumber());
        } finally {
            // Reset original alpha tag and number values.
            mTelephonyManager.setVoiceMailNumber(originalAlphaTag, originalNumber);
        }
    }

    /**
     * This test verifies that {@link SubscriptionManager#createSubscriptionGroup(List)} correctly
     * create a group with the given subscription id.
     *
     * This also verifies that
     * {@link SubscriptionManager#removeSubscriptionsFromGroup(List, ParcelUuid)} correctly remove
     * the given subscription group.
     */
    public void testCreateAndRemoveSubscriptionGroup() {
        if (!hasCellular) return;
        // Set subscription group with current sub Id.
        int subId = SubscriptionManager.getDefaultSubscriptionId();
        List<Integer> subGroup = Arrays.asList(subId);
        ParcelUuid uuid = mSubscriptionManager.createSubscriptionGroup(subGroup);

        // Getting subscriptions in group.
        List<SubscriptionInfo> infoList = mSubscriptionManager.getSubscriptionsInGroup(uuid);

        try {
            assertEquals(1, infoList.size());
            assertEquals(uuid, infoList.get(0).getGroupUuid());
            assertEquals(subId, infoList.get(0).getSubscriptionId());
        } finally {
            // Verify that the given subGroup has been removed.
            mSubscriptionManager.removeSubscriptionsFromGroup(subGroup, uuid);
            infoList = mSubscriptionManager.getSubscriptionsInGroup(uuid);
            assertTrue(infoList.isEmpty());
        }
    }

    public void testAddSubscriptionToExistingGroupForMultipleSims() {
        if (!hasCellular || mTelephonyManager.getPhoneCount() < DSDS_PHONE_COUNT) return;

        // Set subscription group with current sub Id.
        int subId = SubscriptionManager.getDefaultDataSubscriptionId();
        ParcelUuid uuid = mSubscriptionManager.createSubscriptionGroup(Arrays.asList(subId));

        try {
            // Get all active subscriptions.
            List<SubscriptionInfo> activeSubInfos =
                    mSubscriptionManager.getActiveSubscriptionInfoList();

            // Verify that the device has at least two active subscriptions.
            assertTrue(activeSubInfos.size() >= DSDS_PHONE_COUNT);

            List<Integer> activeSubGroup = getSubscriptionIdList(activeSubInfos);
            activeSubGroup.removeIf(id -> id == subId);

            mSubscriptionManager.addSubscriptionsIntoGroup(activeSubGroup, uuid);

            List<Integer> infoList =
                    getSubscriptionIdList(mSubscriptionManager.getSubscriptionsInGroup(uuid));
            activeSubGroup.add(subId);
            assertEquals(activeSubGroup.size(), infoList.size());
            assertTrue(activeSubGroup.containsAll(infoList));
        } finally {
            removeSubscriptionsFromGroup(uuid);
        }
    }

    /**
     * This test verifies that
     * {@link SubscriptionManager#addSubscriptionsIntoGroup(List, ParcelUuid)}} correctly add some
     * additional subscriptions to the existing group.
     *
     * This test required the device has more than one subscription.
     */
    public void testAddSubscriptionToExistingGroupForEsim() {
        if (!hasCellular) return;

        // Set subscription group with current sub Id.
        int subId = SubscriptionManager.getDefaultDataSubscriptionId();
        ParcelUuid uuid = mSubscriptionManager.createSubscriptionGroup(Arrays.asList(subId));

        try {
            // Get all accessible eSim subscription.
            List<SubscriptionInfo> accessibleSubInfos =
                    mSubscriptionManager.getAccessibleSubscriptionInfoList();
            if (accessibleSubInfos != null && accessibleSubInfos.size() > 1) {
                List<Integer> accessibleSubGroup = getSubscriptionIdList(accessibleSubInfos);
                accessibleSubGroup.removeIf(id -> id == subId);

                mSubscriptionManager.addSubscriptionsIntoGroup(accessibleSubGroup, uuid);

                List<Integer> infoList =
                        getSubscriptionIdList(mSubscriptionManager.getSubscriptionsInGroup(uuid));
                accessibleSubGroup.add(subId);
                assertEquals(accessibleSubGroup.size(), infoList.size());
                assertTrue(accessibleSubGroup.containsAll(infoList));
            }
        } finally {
            removeSubscriptionsFromGroup(uuid);
        }
    }

    /**
     * This test verifies that {@link SubscriptionManager#setOpportunistic(boolean, int)} correctly
     * set the opportunistic property of the given subscription.
     */
    public void testOpportunistic() {
        if (!hasCellular) return;

        int subId = SubscriptionManager.getDefaultDataSubscriptionId();
        SubscriptionInfo info = mSubscriptionManager.getActiveSubscriptionInfo(subId);
        boolean oldOpportunistic = info.isOpportunistic();
        boolean newOpportunistic = !oldOpportunistic;

        try {
            // Mark the given subscription as opportunistic subscription.
            boolean successed = mSubscriptionManager.setOpportunistic(newOpportunistic, subId);
            assertTrue(successed);

            // Verify that the given subscription is opportunistic subscription.
            info = mSubscriptionManager.getActiveSubscriptionInfo(subId);
            assertEquals(newOpportunistic, info.isOpportunistic());
        } finally {
            // Set back to original opportunistic property.
            mSubscriptionManager.setOpportunistic(oldOpportunistic, subId);
            info = mSubscriptionManager.getActiveSubscriptionInfo(subId);
            assertEquals(oldOpportunistic, info.isOpportunistic());
        }
    }

    /**
     * This test verifies that {@link TelephonyManager#iccExchangeSimIO(int, int, int, int, int,
     * String)} correctly transmits iccIO commands to the UICC card. First, the MF is selected via a
     * SELECT apdu via the basic channel, then a STATUS AT-command is sent.
     */
    public void testIccExchangeSimIO() {
        if (!hasCellular) return;

        // select the MF first. This makes sure the next STATUS AT-command returns a FCP template
        // for the right file.
        int cla = CLA_SELECT;
        int p1 = 0; // select EF by FID
        int p2 = 0x0C; // requesting FCP template
        int p3 = 2; // length of 'data' payload
        String data = MF_FILE_ID;
        String response = mTelephonyManager
                .iccTransmitApduBasicChannel(cla, COMMAND_SELECT, p1, p2, p3, data);
        assertEquals(STATUS_NORMAL_STRING, response);

        // The iccExchangeSimIO command implements the +CRSM command defined in TS 27.007 section
        // 8.18. A STATUS command is sent and the returned value will be an FCP template.
        byte[] result = mTelephonyManager.iccExchangeSimIO(
                0, // fileId: not required for STATUS
                COMMAND_STATUS,  // command: STATUS
                0, // p1: not required for STATUS
                0, // p2: not required for STATUS
                0, // p3: not required for STATUS
                ""); // filePath: not required for STATUS
        String resultString = bytesToHexString(result);
        FcpTemplate fcpTemplate = FcpTemplate.parseFcpTemplate(resultString);
        assertTrue(containsFileId(fcpTemplate, MF_FILE_ID));
        assertEquals("iccExchangeSimIO returned non-normal Status byte: " + resultString,
                STATUS_NORMAL_STRING, fcpTemplate.getStatus());
    }

    /**
     * This test checks that a STATUS apdu can be sent as an encapsulated envelope to the UICC via
     * {@link TelephonyManager#sendEnvelopeWithStatus(String)}.
     */
    public void testSendEnvelopeWithStatus() {
        if (!hasCellular) return;

        // STATUS apdu as hex String
        String envelope =
                CLA_STATUS_STRING
                + COMMAND_STATUS_STRING
                + "00" // p1: no indication of application status
                + "00"; // p2: identical parameters to
        String response = mTelephonyManager.sendEnvelopeWithStatus(envelope);

        // TODO(b/137963715): add more specific assertions on response from TelMan#sendEnvelope
        assertNotNull("sendEnvelopeWithStatus is null for envelope=" + envelope, response);
    }

    private void verifyValidIccOpenLogicalChannelResponse(IccOpenLogicalChannelResponse response) {
        // The assigned channel should be between the min and max allowed channel numbers
        int channel = response.getChannel();
        assertTrue(MIN_LOGICAL_CHANNEL <= channel && channel <= MAX_LOGICAL_CHANNEL);
        assertEquals(STATUS_NO_ERROR, response.getStatus());
        assertArrayEquals(STATUS_NORMAL, response.getSelectResponse());
    }

    private void removeSubscriptionsFromGroup(ParcelUuid uuid) {
        List<SubscriptionInfo> infoList = mSubscriptionManager.getSubscriptionsInGroup(uuid);
        if (!infoList.isEmpty()) {
            mSubscriptionManager.removeSubscriptionsFromGroup(
                    getSubscriptionIdList(infoList),
                    uuid);
        }
        infoList = mSubscriptionManager.getSubscriptionsInGroup(uuid);
        assertTrue(infoList.isEmpty());
    }

    private List<Integer> getSubscriptionIdList(List<SubscriptionInfo> subInfoList) {
        if (subInfoList == null || subInfoList.isEmpty()) return Collections.EMPTY_LIST;
        return subInfoList.stream()
                .map(info -> info.getSubscriptionId())
                .collect(Collectors.toList());
    }

    /**
     * Checks whether the a {@code fcpTemplate} contains the given {@code fileId}.
     *
     * @param fcpTemplate The FCP Template to be checked.
     * @param fileId The file ID that is being searched for
     *
     * @return true iff fcpTemplate contains fileId.
     */
    private boolean containsFileId(FcpTemplate fcpTemplate, String fileId) {
        return fcpTemplate.getTlvs().stream().anyMatch(tlv ->
                tlv.getTag() == FILE_IDENTIFIER && tlv.getValue().equals(fileId));
    }

    /**
     * Returns true iff {@code response} indicates an error with the previous APDU.
     *
     * @param response The APDU response to be checked.
     *
     * @return true iff the given response indicates an error occurred
     */
    private boolean isErrorResponse(@Nonnull String response) {
        return !(STATUS_NORMAL_STRING.equals(response) ||
            response.startsWith(STATUS_WARNING_A) ||
            response.startsWith(STATUS_WARNING_B) ||
            response.startsWith(STATUS_BYTES_REMAINING));
    }

    private static class IntentReceiver extends BroadcastReceiver {
        private final CountDownLatch mReceiveLatch = new CountDownLatch(1);

        @Override
        public void onReceive(Context context, Intent intent) {
            mReceiveLatch.countDown();
        }

        public boolean waitForReceive() throws InterruptedException {
            return mReceiveLatch.await(30, TimeUnit.SECONDS);
        }
    }

    @Suppress
    public void testEapSimAuthentication() {
        // K: '000102030405060708090A0B0C0D0E0F', defined by TS 134 108#8.2
        // n: 128 (Bits to use for RES value)
        // Format: [Length][RAND]
        String challenge = "10" + EAP_SIM_AKA_RAND;
        String base64Challenge = Base64.encodeToString(hexStringToBytes(challenge), Base64.NO_WRAP);
        String base64Response =
                mTelephonyManager.getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        base64Challenge);
        byte[] response = Base64.decode(base64Response, Base64.DEFAULT);
        assertArrayEquals(
                "Results for AUTHTYPE_EAP_SIM failed",
                hexStringToBytes(EXPECTED_EAP_SIM_RESULT),
                response);
    }

    @Suppress
    public void testEapAkaAuthentication() {
        // K: '000102030405060708090A0B0C0D0E0F', defined by TS 134 108#8.2
        // n: 128 (Bits to use for RES value)
        // Format: [Length][Rand][Length][Autn]
        String challenge = "10" + EAP_SIM_AKA_RAND + "10" + EAP_AKA_AUTN;
        String base64Challenge = Base64.encodeToString(hexStringToBytes(challenge), Base64.NO_WRAP);
        String base64Response =
                mTelephonyManager.getIccAuthentication(
                        TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA,
                        base64Challenge);

        assertNotNull("UICC returned null for EAP-AKA auth", base64Response);
        byte[] response = Base64.decode(base64Response, Base64.NO_WRAP);

        // response may be formatted as: [DB][Length][RES][Length][CK][Length][IK][Length][Kc]
        byte[] akaResponse = Arrays.copyOfRange(response, 0, EAP_AKA_RESPONSE_LENGTH);
        assertArrayEquals(
                "Results for AUTHTYPE_EAP_AKA failed",
                hexStringToBytes(EXPECTED_EAP_AKA_RESULT),
                akaResponse);
    }
}
