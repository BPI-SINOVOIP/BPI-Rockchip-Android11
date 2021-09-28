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
 * limitations under the License.
 */

package android.telephony.euicc.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.telephony.TelephonyManager;
import android.telephony.euicc.DownloadableSubscription;
import android.telephony.euicc.EuiccCardManager;
import android.telephony.euicc.EuiccInfo;
import android.telephony.euicc.EuiccManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class EuiccManagerTest {

    private static final int REQUEST_CODE = 0;
    private static final int CALLBACK_TIMEOUT_MILLIS = 2000;
    // starting activities might take extra time
    private static final int ACTIVITY_CALLBACK_TIMEOUT_MILLIS = 5000;
    private static final String ACTION_DOWNLOAD_SUBSCRIPTION = "cts_download_subscription";
    private static final String ACTION_DELETE_SUBSCRIPTION = "cts_delete_subscription";
    private static final String ACTION_SWITCH_TO_SUBSCRIPTION = "cts_switch_to_subscription";
    private static final String ACTION_ERASE_SUBSCRIPTIONS = "cts_erase_subscriptions";
    private static final String ACTION_START_TEST_RESOLUTION_ACTIVITY =
            "cts_start_test_resolution_activity";
    private static final String ACTIVATION_CODE = "1$LOCALHOST$04386-AGYFT-A74Y8-3F815";

    private static final String[] sCallbackActions =
            new String[]{
                    ACTION_DOWNLOAD_SUBSCRIPTION,
                    ACTION_DELETE_SUBSCRIPTION,
                    ACTION_SWITCH_TO_SUBSCRIPTION,
                    ACTION_ERASE_SUBSCRIPTIONS,
                    ACTION_START_TEST_RESOLUTION_ACTIVITY,
            };

    private EuiccManager mEuiccManager;
    private CallbackReceiver mCallbackReceiver;

    @Before
    public void setUp() throws Exception {
        mEuiccManager = (EuiccManager) getContext().getSystemService(Context.EUICC_SERVICE);
    }

    @After
    public void tearDown() throws Exception {
        if (mCallbackReceiver != null) {
            getContext().unregisterReceiver(mCallbackReceiver);
        }
    }

    @Test
    public void testGetEid() {
        // test disabled state only for now
        if (mEuiccManager.isEnabled()) {
            return;
        }

        // call getEid()
        String eid = mEuiccManager.getEid();

        // verify result is null
        assertNull(eid);
    }

    @Test
    public void testCreateForCardId() {
        // just verify that this does not crash
        mEuiccManager = mEuiccManager.createForCardId(TelephonyManager.UNINITIALIZED_CARD_ID);
        mEuiccManager = mEuiccManager.createForCardId(TelephonyManager.UNSUPPORTED_CARD_ID);
    }

    @Test
    public void testDownloadSubscription() {
        // test disabled state only for now
        if (mEuiccManager.isEnabled()) {
            return;
        }

        // set up CountDownLatch and receiver
        CountDownLatch countDownLatch = new CountDownLatch(1);
        mCallbackReceiver = new CallbackReceiver(countDownLatch);
        getContext()
                .registerReceiver(
                        mCallbackReceiver, new IntentFilter(ACTION_DOWNLOAD_SUBSCRIPTION));

        // call downloadSubscription()
        DownloadableSubscription subscription = createDownloadableSubscription();
        PendingIntent callbackIntent = createCallbackIntent(ACTION_DOWNLOAD_SUBSCRIPTION);
        mEuiccManager.downloadSubscription(
                subscription, false /* switchAfterDownload */, callbackIntent);

        // wait for callback
        try {
            countDownLatch.await(CALLBACK_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.toString());
        }

        // verify correct result code is received
        assertEquals(
                EuiccManager.EMBEDDED_SUBSCRIPTION_RESULT_ERROR, mCallbackReceiver.getResultCode());
    }

    @Test
    public void testGetEuiccInfo() {
        // test disabled state only for now
        if (mEuiccManager.isEnabled()) {
            return;
        }

        // call getEuiccInfo()
        EuiccInfo euiccInfo = mEuiccManager.getEuiccInfo();

        // verify result is null
        assertNull(euiccInfo);
    }

    @Test
    public void testDeleteSubscription() {
        // test disabled state only for now
        if (mEuiccManager.isEnabled()) {
            return;
        }

        // set up CountDownLatch and receiver
        CountDownLatch countDownLatch = new CountDownLatch(1);
        mCallbackReceiver = new CallbackReceiver(countDownLatch);
        getContext()
                .registerReceiver(mCallbackReceiver, new IntentFilter(ACTION_DELETE_SUBSCRIPTION));

        // call deleteSubscription()
        PendingIntent callbackIntent = createCallbackIntent(ACTION_DELETE_SUBSCRIPTION);
        mEuiccManager.deleteSubscription(3, callbackIntent);

        // wait for callback
        try {
            countDownLatch.await(CALLBACK_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.toString());
        }

        // verify correct result code is received
        assertEquals(
                EuiccManager.EMBEDDED_SUBSCRIPTION_RESULT_ERROR, mCallbackReceiver.getResultCode());
    }

    @Test
    public void testSwitchToSubscription() {
        // test disabled state only for now
        if (mEuiccManager.isEnabled()) {
            return;
        }

        // set up CountDownLatch and receiver
        CountDownLatch countDownLatch = new CountDownLatch(1);
        mCallbackReceiver = new CallbackReceiver(countDownLatch);
        getContext()
                .registerReceiver(
                        mCallbackReceiver, new IntentFilter(ACTION_SWITCH_TO_SUBSCRIPTION));

        // call deleteSubscription()
        PendingIntent callbackIntent = createCallbackIntent(ACTION_SWITCH_TO_SUBSCRIPTION);
        mEuiccManager.switchToSubscription(4, callbackIntent);

        // wait for callback
        try {
            countDownLatch.await(CALLBACK_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.toString());
        }

        // verify correct result code is received
        assertEquals(
                EuiccManager.EMBEDDED_SUBSCRIPTION_RESULT_ERROR, mCallbackReceiver.getResultCode());
    }

    @Test
    public void testEraseSubscriptions() {
        // test disabled state only for now
        if (mEuiccManager.isEnabled()) {
            return;
        }

        // set up CountDownLatch and receiver
        CountDownLatch countDownLatch = new CountDownLatch(1);
        mCallbackReceiver = new CallbackReceiver(countDownLatch);
        getContext()
                .registerReceiver(
                        mCallbackReceiver, new IntentFilter(ACTION_ERASE_SUBSCRIPTIONS));

        // call eraseSubscriptions()
        PendingIntent callbackIntent = createCallbackIntent(ACTION_ERASE_SUBSCRIPTIONS);
        mEuiccManager.eraseSubscriptions(EuiccCardManager.RESET_OPTION_DELETE_OPERATIONAL_PROFILES,
                callbackIntent);

        // wait for callback
        try {
            countDownLatch.await(CALLBACK_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.toString());
        }

        // verify correct result code is received
        assertEquals(
                EuiccManager.EMBEDDED_SUBSCRIPTION_RESULT_ERROR, mCallbackReceiver.getResultCode());
    }

    @Test
    public void testStartResolutionActivity() {
        // set up CountDownLatch and receiver
        CountDownLatch countDownLatch = new CountDownLatch(1);
        mCallbackReceiver = new CallbackReceiver(countDownLatch);
        getContext()
                .registerReceiver(
                        mCallbackReceiver, new IntentFilter(ACTION_START_TEST_RESOLUTION_ACTIVITY));

        /*
         * Start EuiccTestResolutionActivity to test EuiccManager#startResolutionActivity(), since
         * it requires a foreground activity. EuiccTestResolutionActivity will report the test
         * result to the callback receiver.
         */
        Intent testResolutionActivityIntent =
                new Intent(getContext(), EuiccTestResolutionActivity.class);
        PendingIntent callbackIntent = createCallbackIntent(ACTION_START_TEST_RESOLUTION_ACTIVITY);
        testResolutionActivityIntent.putExtra(
                EuiccTestResolutionActivity.EXTRA_ACTIVITY_CALLBACK_INTENT, callbackIntent);
        testResolutionActivityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        getContext().startActivity(testResolutionActivityIntent);

        // wait for callback
        try {
            countDownLatch.await(ACTIVITY_CALLBACK_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.toString());
        }

        // verify test result reported by EuiccTestResolutionActivity
        assertEquals(
                EuiccTestResolutionActivity.RESULT_CODE_TEST_PASSED,
                mCallbackReceiver.getResultCode());
    }

    @Test
    public void testOperationCode() {
        // Ensure if platform source code is updated, these constants stays the same.
        assertEquals(EuiccManager.OPERATION_SYSTEM, 1);
        assertEquals(EuiccManager.OPERATION_SIM_SLOT, 2);
        assertEquals(EuiccManager.OPERATION_EUICC_CARD, 3);
        assertEquals(EuiccManager.OPERATION_SWITCH, 4);
        assertEquals(EuiccManager.OPERATION_DOWNLOAD, 5);
        assertEquals(EuiccManager.OPERATION_METADATA, 6);
        assertEquals(EuiccManager.OPERATION_EUICC_GSMA, 7);
        assertEquals(EuiccManager.OPERATION_APDU, 8);
        assertEquals(EuiccManager.OPERATION_SMDX, 9);
        assertEquals(EuiccManager.OPERATION_SMDX_SUBJECT_REASON_CODE, 10);
        assertEquals(EuiccManager.OPERATION_HTTP, 11);
    }

    @Test
    public void testErrorCode() {
        // Ensure if platform source code is updated, these constants stays the same.
        assertEquals(EuiccManager.ERROR_CARRIER_LOCKED, 10000);
        assertEquals(EuiccManager.ERROR_INVALID_ACTIVATION_CODE, 10001);
        assertEquals(EuiccManager.ERROR_INVALID_CONFIRMATION_CODE, 10002);
        assertEquals(EuiccManager.ERROR_INCOMPATIBLE_CARRIER, 10003);
        assertEquals(EuiccManager.ERROR_EUICC_INSUFFICIENT_MEMORY, 10004);
        assertEquals(EuiccManager.ERROR_TIME_OUT, 10005);
        assertEquals(EuiccManager.ERROR_EUICC_MISSING, 10006);
        assertEquals(EuiccManager.ERROR_UNSUPPORTED_VERSION, 10007);
        assertEquals(EuiccManager.ERROR_SIM_MISSING, 10008);
        assertEquals(EuiccManager.ERROR_INSTALL_PROFILE, 10009);
        assertEquals(EuiccManager.ERROR_DISALLOWED_BY_PPR, 10010);
        assertEquals(EuiccManager.ERROR_ADDRESS_MISSING, 10011);
        assertEquals(EuiccManager.ERROR_CERTIFICATE_ERROR, 10012);
        assertEquals(EuiccManager.ERROR_NO_PROFILES_AVAILABLE, 10013);
        assertEquals(EuiccManager.ERROR_CONNECTION_ERROR, 10014);
        assertEquals(EuiccManager.ERROR_INVALID_RESPONSE, 10015);
        assertEquals(EuiccManager.ERROR_OPERATION_BUSY, 10016);
    }

    @Test
    public void testSetSupportedCountries() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalSupportedCountry = mEuiccManager.getSupportedCountries();

        List<String> expectedCountries = Arrays.asList("US", "SG");
        // Sets supported countries
        mEuiccManager.setSupportedCountries(expectedCountries);

        // Verify supported countries are expected
        assertEquals(expectedCountries, mEuiccManager.getSupportedCountries());

        // Restore the original country list
        mEuiccManager.setSupportedCountries(originalSupportedCountry);
    }

    @Test
    public void testSetUnsupportedCountries() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalUnsupportedCountry = mEuiccManager.getUnsupportedCountries();

        List<String> expectedCountries = Arrays.asList("US", "SG");
        // Sets unsupported countries
        mEuiccManager.setUnsupportedCountries(expectedCountries);

        // Verify unsupported countries are expected
        assertEquals(expectedCountries, mEuiccManager.getUnsupportedCountries());

        // Restore the original country list
        mEuiccManager.setUnsupportedCountries(originalUnsupportedCountry);
    }

    @Test
    public void testIsSupportedCountry_returnsTrue_ifCountryIsOnSupportedList() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalSupportedCountry = mEuiccManager.getSupportedCountries();

        // Sets supported countries
        mEuiccManager.setSupportedCountries(Arrays.asList("US", "SG"));

        // Verify the country is supported
        assertTrue(mEuiccManager.isSupportedCountry("US"));

        // Restore the original country list
        mEuiccManager.setSupportedCountries(originalSupportedCountry);
    }

    @Test
    public void testIsSupportedCountry_returnsTrue_ifCountryIsNotOnUnsupportedList() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalSupportedCountry = mEuiccManager.getSupportedCountries();
        List<String> originalUnsupportedCountry = mEuiccManager.getUnsupportedCountries();

        // Sets supported countries
        mEuiccManager.setSupportedCountries(new ArrayList<>());
        // Sets unsupported countries
        mEuiccManager.setUnsupportedCountries(Arrays.asList("SG"));

        // Verify the country is supported
        assertTrue(mEuiccManager.isSupportedCountry("US"));

        // Restore the original country list
        mEuiccManager.setSupportedCountries(originalSupportedCountry);
        mEuiccManager.setUnsupportedCountries(originalUnsupportedCountry);
    }

    @Test
    public void testIsSupportedCountry_returnsFalse_ifCountryIsNotOnSupportedList() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalSupportedCountry = mEuiccManager.getSupportedCountries();

        // Sets supported countries
        mEuiccManager.setSupportedCountries(Arrays.asList("SG"));

        // Verify the country is not supported
        assertFalse(mEuiccManager.isSupportedCountry("US"));

        // Restore the original country list
        mEuiccManager.setSupportedCountries(originalSupportedCountry);
    }

    @Test
    public void testIsSupportedCountry_returnsFalse_ifCountryIsOnUnsupportedList() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalSupportedCountry = mEuiccManager.getSupportedCountries();
        List<String> originalUnsupportedCountry = mEuiccManager.getUnsupportedCountries();

        // Sets supported countries
        mEuiccManager.setSupportedCountries(new ArrayList<>());
        // Sets unsupported countries
        mEuiccManager.setUnsupportedCountries(Arrays.asList("US"));

        // Verify the country is not supported
        assertFalse(mEuiccManager.isSupportedCountry("US"));

        // Restore the original country list
        mEuiccManager.setSupportedCountries(originalSupportedCountry);
        mEuiccManager.setUnsupportedCountries(originalUnsupportedCountry);
    }

    @Test
    public void testIsSupportedCountry_returnsFalse_ifBothListsAreEmpty() {
        // Only test it when EuiccManager is enabled.
        if (!mEuiccManager.isEnabled()) {
            return;
        }

        // Get country list for restoring later.
        List<String> originalSupportedCountry = mEuiccManager.getSupportedCountries();
        List<String> originalUnsupportedCountry = mEuiccManager.getUnsupportedCountries();

        // Sets supported countries
        mEuiccManager.setSupportedCountries(new ArrayList<>());
        // Sets unsupported countries
        mEuiccManager.setUnsupportedCountries(new ArrayList<>());

        // Verify the country is supported
        assertTrue(mEuiccManager.isSupportedCountry("US"));

        // Restore the original country list
        mEuiccManager.setSupportedCountries(originalSupportedCountry);
        mEuiccManager.setUnsupportedCountries(originalUnsupportedCountry);
    }

    private Context getContext() {
        return InstrumentationRegistry.getContext();
    }

    private DownloadableSubscription createDownloadableSubscription() {
        return DownloadableSubscription.forActivationCode(ACTIVATION_CODE);
    }

    private PendingIntent createCallbackIntent(String action) {
        Intent intent = new Intent(action);
        return PendingIntent.getBroadcast(
                getContext(), REQUEST_CODE, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static class CallbackReceiver extends BroadcastReceiver {

        private CountDownLatch mCountDownLatch;

        public CallbackReceiver(CountDownLatch latch) {
            mCountDownLatch = latch;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            for (String callbackAction : sCallbackActions) {
                if (callbackAction.equals(intent.getAction())) {
                    int resultCode = getResultCode();
                    mCountDownLatch.countDown();
                    break;
                }
            }
        }
    }
}
