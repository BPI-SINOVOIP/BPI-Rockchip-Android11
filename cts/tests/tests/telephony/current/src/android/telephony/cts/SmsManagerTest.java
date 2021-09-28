/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.telephony.cts;

import static androidx.test.InstrumentationRegistry.getContext;
import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.BlockedNumberUtil.deleteBlockedNumber;
import static com.android.compatibility.common.util.BlockedNumberUtil.insertBlockedNumber;

import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.emptyString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.startsWith;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.app.AppOpsManager;
import android.app.PendingIntent;
import android.app.UiAutomation;
import android.app.role.RoleManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.RemoteCallback;
import android.os.SystemClock;
import android.provider.Telephony;
import android.telephony.SmsCbMessage;
import android.telephony.SmsMessage;
import android.telephony.TelephonyManager;
import android.telephony.cdma.CdmaSmsCbProgramData;
import android.text.TextUtils;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link android.telephony.SmsManager}.
 *
 * Structured so tests can be reused to test {@link android.telephony.gsm.SmsManager}
 */
public class SmsManagerTest {

    private static final String TAG = "SmsManagerTest";
    private static final String LONG_TEXT =
        "This is a very long text. This text should be broken into three " +
        "separate messages.This is a very long text. This text should be broken into " +
        "three separate messages.This is a very long text. This text should be broken " +
        "into three separate messages.This is a very long text. This text should be " +
        "broken into three separate messages.";;
    private static final String LONG_TEXT_WITH_32BIT_CHARS =
        "Long dkkshsh jdjsusj kbsksbdf jfkhcu hhdiwoqiwyrygrvn?*?*!\";:'/,."
        + "__?9#9292736&4;\"$+$+((]\\[\\℅©℅™^®°¥°¥=¢£}}£∆~¶~÷|√×."
        + " 😯😆😉😇😂😀👕🎓😀👙🐕🐀🐶🐰🐩⛪⛲ ";

    private static final String SMS_SEND_ACTION = "CTS_SMS_SEND_ACTION";
    private static final String SMS_DELIVERY_ACTION = "CTS_SMS_DELIVERY_ACTION";
    private static final String DATA_SMS_RECEIVED_ACTION = "android.intent.action.DATA_SMS_RECEIVED";
    public static final String SMS_DELIVER_DEFAULT_APP_ACTION = "CTS_SMS_DELIVERY_ACTION_DEFAULT_APP";
    public static final String LEGACY_SMS_APP = "android.telephony.cts.sms23";
    public static final String MODERN_SMS_APP = "android.telephony.cts.sms";
    private static final String SMS_RETRIEVER_APP = "android.telephony.cts.smsretriever";
    private static final String SMS_RETRIEVER_ACTION = "CTS_SMS_RETRIEVER_ACTION";
    private static final String FINANCIAL_SMS_APP = "android.telephony.cts.financialsms";

    private TelephonyManager mTelephonyManager;
    private PackageManager mPackageManager;
    private String mDestAddr;
    private String mText;
    private SmsBroadcastReceiver mSendReceiver;
    private SmsBroadcastReceiver mDeliveryReceiver;
    private SmsBroadcastReceiver mDataSmsReceiver;
    private SmsBroadcastReceiver mSmsDeliverReceiver;
    private SmsBroadcastReceiver mSmsReceivedReceiver;
    private SmsBroadcastReceiver mSmsRetrieverReceiver;
    private PendingIntent mSentIntent;
    private PendingIntent mDeliveredIntent;
    private Intent mSendIntent;
    private Intent mDeliveryIntent;
    private Context mContext;
    private Uri mBlockedNumberUri;
    private boolean mTestAppSetAsDefaultSmsApp;
    private boolean mDeliveryReportSupported;
    private static boolean mReceivedDataSms;
    private static String mReceivedText;
    private static boolean sHasShellPermissionIdentity = false;
    private static long sMessageId = 0L;

    private static final int TIME_OUT = 1000 * 60 * 4;
    private static final int NO_CALLS_TIMEOUT_MILLIS = 1000; // 1 second

    @Before
    public void setUp() throws Exception {
        assumeTrue(InstrumentationRegistry.getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TELEPHONY));

        mContext = getContext();
        mTelephonyManager =
            (TelephonyManager) getContext().getSystemService(
                    Context.TELEPHONY_SERVICE);
        mPackageManager = mContext.getPackageManager();
        mDestAddr = mTelephonyManager.getLine1Number();
        mText = "This is a test message";

        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            mDeliveryReportSupported = false;
        } else {
            // exclude the networks that don't support SMS delivery report
            String mccmnc = mTelephonyManager.getSimOperator();
            mDeliveryReportSupported = !(CarrierCapability.NO_DELIVERY_REPORTS.contains(mccmnc));
        }
    }

    @After
    public void tearDown() throws Exception {
        if (mBlockedNumberUri != null) {
            unblockNumber(mBlockedNumberUri);
            mBlockedNumberUri = null;
        }
        if (mTestAppSetAsDefaultSmsApp) {
            setDefaultSmsApp(false);
        }
    }

    @Test
    public void testDivideMessage() {
        ArrayList<String> dividedMessages = divideMessage(LONG_TEXT);
        assertNotNull(dividedMessages);
        if (TelephonyUtils.isSkt(mTelephonyManager)) {
            assertTrue(isComplete(dividedMessages, 5, LONG_TEXT)
                    || isComplete(dividedMessages, 3, LONG_TEXT));
        } else if (TelephonyUtils.isKt(mTelephonyManager)) {
            assertTrue(isComplete(dividedMessages, 4, LONG_TEXT)
                    || isComplete(dividedMessages, 3, LONG_TEXT));
        } else {
            assertTrue(isComplete(dividedMessages, 3, LONG_TEXT));
        }
    }

    @Test
    public void testDivideUnicodeMessage() {
        ArrayList<String> dividedMessages = divideMessage(LONG_TEXT_WITH_32BIT_CHARS);
        assertNotNull(dividedMessages);
        assertTrue(isComplete(dividedMessages, 3, LONG_TEXT_WITH_32BIT_CHARS));
        for (String messagePiece : dividedMessages) {
            assertFalse(Character.isHighSurrogate(
                    messagePiece.charAt(messagePiece.length() - 1)));
        }
    }

    private boolean isComplete(List<String> dividedMessages, int numParts, String longText) {
        if (dividedMessages.size() != numParts) {
            return false;
        }

        String actualMessage = "";
        for (int i = 0; i < numParts; i++) {
            actualMessage += dividedMessages.get(i);
        }
        return longText.equals(actualMessage);
    }

    @Test
    public void testSmsRetriever() throws Exception {
        assertFalse("[RERUN] SIM card does not provide phone number. Use a suitable SIM Card.",
                TextUtils.isEmpty(mDestAddr));

        String mccmnc = mTelephonyManager.getSimOperator();
        setupBroadcastReceivers();
        init();

        CompletableFuture<Bundle> callbackResult = new CompletableFuture<>();

        mContext.startActivity(new Intent()
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .setComponent(new ComponentName(
                        SMS_RETRIEVER_APP, SMS_RETRIEVER_APP + ".MainActivity"))
                .putExtra("callback", new RemoteCallback(callbackResult::complete)));


        Bundle bundle = callbackResult.get(200, TimeUnit.SECONDS);
        String token = bundle.getString("token");
        assertThat(bundle.getString("class"), startsWith(SMS_RETRIEVER_APP));
        assertNotNull(token);

        String composedText = "testprefix1" + mText + token;
        sendTextMessage(mDestAddr, composedText, null, null);

        assertTrue("[RERUN] SMS retriever message not received. Check signal.",
                mSmsRetrieverReceiver.waitForCalls(1, TIME_OUT));
    }

    private void sendAndReceiveSms(boolean addMessageId) throws Exception {
        // send single text sms
        init();
        if (addMessageId) {
            long fakeMessageId = 19812L;
            sendTextMessageWithMessageId(mDestAddr, mDestAddr, mSentIntent, mDeliveredIntent,
                    fakeMessageId);
        } else {
            sendTextMessage(mDestAddr, mDestAddr, mSentIntent, mDeliveredIntent);
        }
        assertTrue("[RERUN] Could not send SMS. Check signal.",
                mSendReceiver.waitForCalls(1, TIME_OUT));
        if (mDeliveryReportSupported) {
            assertTrue("[RERUN] SMS message delivery notification not received. Check signal.",
                    mDeliveryReceiver.waitForCalls(1, TIME_OUT));
        }
        // non-default app should receive only SMS_RECEIVED_ACTION
        assertTrue(mSmsReceivedReceiver.waitForCalls(1, TIME_OUT));
        // Received SMS should always contain a generated messageId
        assertNotEquals(0L, sMessageId);
        assertTrue(mSmsDeliverReceiver.waitForCalls(0, 0));
    }

    private void sendAndReceiveMultipartSms(String mccmnc, boolean addMessageId) throws Exception {
        sMessageId = 0L;
        int numPartsSent = sendMultipartTextMessageIfSupported(mccmnc, addMessageId);
        if (numPartsSent > 0) {
            assertTrue("[RERUN] Could not send multi part SMS. Check signal.",
                    mSendReceiver.waitForCalls(numPartsSent, TIME_OUT));
            if (mDeliveryReportSupported) {
                assertTrue("[RERUN] Multi part SMS message delivery notification not received. "
                        + "Check signal.", mDeliveryReceiver.waitForCalls(numPartsSent, TIME_OUT));
            }
            // non-default app should receive only SMS_RECEIVED_ACTION
            assertTrue(mSmsReceivedReceiver.waitForCalls(1, TIME_OUT));
            assertTrue(mSmsDeliverReceiver.waitForCalls(0, 0));
            // Received SMS should contain a generated messageId
            assertNotEquals(0L, sMessageId);
        } else {
            // This GSM network doesn't support Multipart SMS message.
            // Skip the test.
        }
    }

    private void sendDataSms(String mccmnc) throws Exception {
        if (sendDataMessageIfSupported(mccmnc)) {
            assertTrue("[RERUN] Could not send data SMS. Check signal.",
                    mSendReceiver.waitForCalls(1, TIME_OUT));
            if (mDeliveryReportSupported) {
                assertTrue("[RERUN] Data SMS message delivery notification not received. " +
                        "Check signal.", mDeliveryReceiver.waitForCalls(1, TIME_OUT));
            }
            mDataSmsReceiver.waitForCalls(1, TIME_OUT);
            assertTrue("[RERUN] Data SMS message not received. Check signal.", mReceivedDataSms);
            assertEquals(mReceivedText, mText);
        } else {
            // This GSM network doesn't support Data(binary) SMS message.
            // Skip the test.
        }
    }

    @Test
    public void testSendAndReceiveMessages() throws Exception {
        assertFalse("[RERUN] SIM card does not provide phone number. Use a suitable SIM Card.",
                TextUtils.isEmpty(mDestAddr));

        String mccmnc = mTelephonyManager.getSimOperator();
        setupBroadcastReceivers();

        // send/receive single text sms with and without messageId
        sendAndReceiveSms(/* addMessageId= */ true);
        sendAndReceiveSms(/* addMessageId= */ false);

        // due to permission restrictions, currently there is no way to make this test app the
        // default SMS app

        if (mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
            // TODO: temp workaround, OCTET encoding for EMS not properly supported
            return;
        }

        // send/receive data sms
        sendDataSms(mccmnc);

        // send/receive multi part text sms with and without messageId
        sendAndReceiveMultipartSms(mccmnc, /* addMessageId= */ true);
        sendAndReceiveMultipartSms(mccmnc, /* addMessageId= */ false);
    }

    @Test
    public void testSmsBlocking() throws Exception {
        assertFalse("[RERUN] SIM card does not provide phone number. Use a suitable SIM Card.",
                TextUtils.isEmpty(mDestAddr));

        // disable suppressing blocking.
        TelephonyUtils.endBlockSuppression(getInstrumentation());

        String mccmnc = mTelephonyManager.getSimOperator();
        // Setting default SMS App is needed to be able to block numbers.
        setDefaultSmsApp(true);
        blockNumber(mDestAddr);
        setupBroadcastReceivers();

        // single-part SMS blocking
        init();
        sendTextMessage(mDestAddr, mDestAddr, mSentIntent, mDeliveredIntent);
        assertTrue("[RERUN] Could not send SMS. Check signal.",
                mSendReceiver.waitForCalls(1, TIME_OUT));
        assertTrue("Expected no messages to be received due to number blocking.",
                mSmsReceivedReceiver.verifyNoCalls(NO_CALLS_TIMEOUT_MILLIS));
        assertTrue("Expected no messages to be delivered due to number blocking.",
                mSmsDeliverReceiver.verifyNoCalls(NO_CALLS_TIMEOUT_MILLIS));

        // send data sms
        if (!sendDataMessageIfSupported(mccmnc)) {
            assertTrue("[RERUN] Could not send data SMS. Check signal.",
                    mSendReceiver.waitForCalls(1, TIME_OUT));
            if (mDeliveryReportSupported) {
                assertTrue("[RERUN] Data SMS message delivery notification not received. " +
                        "Check signal.", mDeliveryReceiver.waitForCalls(1, TIME_OUT));
            }
            assertTrue("Expected no messages to be delivered due to number blocking.",
                    mSmsDeliverReceiver.verifyNoCalls(NO_CALLS_TIMEOUT_MILLIS));
        } else {
            // This GSM network doesn't support Data(binary) SMS message.
            // Skip the test.
        }

        // multi-part SMS blocking
        int numPartsSent = sendMultipartTextMessageIfSupported(mccmnc, /* addMessageId= */ false);
        if (numPartsSent > 0) {
            assertTrue("[RERUN] Could not send multi part SMS. Check signal.",
                    mSendReceiver.waitForCalls(numPartsSent, TIME_OUT));

            assertTrue("Expected no messages to be received due to number blocking.",
                    mSmsReceivedReceiver.verifyNoCalls(NO_CALLS_TIMEOUT_MILLIS));
            assertTrue("Expected no messages to be delivered due to number blocking.",
                    mSmsDeliverReceiver.verifyNoCalls(NO_CALLS_TIMEOUT_MILLIS));
        } else {
            // This GSM network doesn't support Multipart SMS message.
            // Skip the test.
        }
    }

    @Test
    public void testGetSmsMessagesForFinancialAppPermissionRequestedNotGranted() throws Exception {
        CompletableFuture<Bundle> callbackResult = new CompletableFuture<>();

        mContext.startActivity(new Intent()
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .setComponent(new ComponentName(FINANCIAL_SMS_APP, FINANCIAL_SMS_APP + ".MainActivity"))
                .putExtra("callback", new RemoteCallback(callbackResult::complete)));

        Bundle bundle = callbackResult.get(500, TimeUnit.SECONDS);

        assertThat(bundle.getString("class"), startsWith(FINANCIAL_SMS_APP));
        assertThat(bundle.getInt("rowNum"), equalTo(-1));
    }

    @Test
    public void testGetSmsMessagesForFinancialAppPermissionRequestedGranted() throws Exception {
        CompletableFuture<Bundle> callbackResult = new CompletableFuture<>();
        String ctsPackageName = getInstrumentation().getContext().getPackageName();

        executeWithShellPermissionIdentity(() -> {
            setModeForOps(FINANCIAL_SMS_APP,
                    AppOpsManager.MODE_ALLOWED,
                    AppOpsManager.OPSTR_SMS_FINANCIAL_TRANSACTIONS);
            });
        mContext.startActivity(new Intent()
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .setComponent(new ComponentName(FINANCIAL_SMS_APP, FINANCIAL_SMS_APP + ".MainActivity"))
                .putExtra("callback", new RemoteCallback(callbackResult::complete)));


        Bundle bundle = callbackResult.get(500, TimeUnit.SECONDS);

        assertThat(bundle.getString("class"), startsWith(FINANCIAL_SMS_APP));
        assertThat(bundle.getInt("rowNum"), equalTo(-1));
    }

    @Test
    public void testSmsNotPersisted_failsWithoutCarrierPermissions() throws Exception {
        assertFalse("[RERUN] SIM card does not provide phone number. Use a suitable SIM Card.",
                TextUtils.isEmpty(mDestAddr));

        try {
            getSmsManager().sendTextMessageWithoutPersisting(mDestAddr, null /*scAddress */,
                    mDestAddr, mSentIntent, mDeliveredIntent);
            fail("We should get a SecurityException due to not having carrier privileges");
        } catch (SecurityException e) {
            // Success
        }
    }

    @Test
    public void testContentProviderAccessRestriction() throws Exception {
        Uri dummySmsUri = null;
        Context context = getInstrumentation().getContext();
        ContentResolver contentResolver = context.getContentResolver();
        int originalWriteSmsMode = -1;
        String ctsPackageName = context.getPackageName();
        try {
            // Insert some dummy sms
            originalWriteSmsMode = context.getSystemService(AppOpsManager.class)
                    .unsafeCheckOpNoThrow(AppOpsManager.OPSTR_WRITE_SMS,
                            getPackageUid(ctsPackageName), ctsPackageName);
            dummySmsUri = executeWithShellPermissionIdentity(() -> {
                setModeForOps(ctsPackageName,
                        AppOpsManager.MODE_ALLOWED, AppOpsManager.OPSTR_WRITE_SMS);
                ContentValues contentValues = new ContentValues();
                contentValues.put(Telephony.TextBasedSmsColumns.ADDRESS, "addr");
                contentValues.put(Telephony.TextBasedSmsColumns.READ, 1);
                contentValues.put(Telephony.TextBasedSmsColumns.SUBJECT, "subj");
                contentValues.put(Telephony.TextBasedSmsColumns.BODY, "created_at_" +
                        new Date().toString().replace(" ", "_"));
                return contentResolver.insert(Telephony.Sms.CONTENT_URI, contentValues);
            });
            assertNotNull("Failed to insert dummy sms", dummySmsUri);
            assertNotEquals("Failed to insert dummy sms", dummySmsUri.getLastPathSegment(), "0");
            testSmsAccessAboutDefaultApp(LEGACY_SMS_APP);
            testSmsAccessAboutDefaultApp(MODERN_SMS_APP);
        } finally {
            if (dummySmsUri != null && !"/0".equals(dummySmsUri.getLastPathSegment())) {
                final Uri finalDummySmsUri = dummySmsUri;
                executeWithShellPermissionIdentity(() -> contentResolver.delete(finalDummySmsUri,
                        null, null));
            }
            if (originalWriteSmsMode >= 0) {
                int finalOriginalWriteSmsMode = originalWriteSmsMode;
                executeWithShellPermissionIdentity(() ->
                        setModeForOps(ctsPackageName,
                                finalOriginalWriteSmsMode, AppOpsManager.OPSTR_WRITE_SMS));
            }
        }
    }

    private void testSmsAccessAboutDefaultApp(String pkg)
            throws Exception {
        String originalSmsApp = getSmsApp();
        assertNotEquals(pkg, originalSmsApp);
        assertCanAccessSms(pkg);
        try {
            setSmsApp(pkg);
            assertCanAccessSms(pkg);
        } finally {
            resetReadWriteSmsAppOps(pkg);
            setSmsApp(originalSmsApp);
        }
    }

    private void resetReadWriteSmsAppOps(String pkg) throws Exception {
        setModeForOps(pkg, AppOpsManager.MODE_DEFAULT,
                AppOpsManager.OPSTR_READ_SMS, AppOpsManager.OPSTR_WRITE_SMS);
    }

    private void setModeForOps(String pkg, int mode, String... ops) throws Exception {
        // We cannot reset these app ops to DEFAULT via current API, so we reset them manually here
        // temporarily as we will rewrite how the default SMS app is setup later.
        executeWithShellPermissionIdentity(() -> {
            int uid = getPackageUid(pkg);
            AppOpsManager appOpsManager =
                    getInstrumentation().getContext().getSystemService(AppOpsManager.class);
            for (String op : ops) {
                appOpsManager.setUidMode(op, uid, mode);
            }
        });
    }

    private int getPackageUid(String pkg) throws PackageManager.NameNotFoundException {
        return getInstrumentation().getContext().getPackageManager().getPackageUid(pkg, 0);
    }

    private String getSmsApp() throws Exception {
        return executeWithShellPermissionIdentity(() -> getInstrumentation()
                .getContext()
                .getSystemService(RoleManager.class)
                .getRoleHolders(RoleManager.ROLE_SMS)
                .get(0));
    }

    private void setSmsApp(String pkg) throws Exception {
        executeWithShellPermissionIdentity(() -> {
            Context context = getInstrumentation().getContext();
            CompletableFuture<Boolean> result = new CompletableFuture<>();
            context.getSystemService(RoleManager.class).addRoleHolderAsUser(
                    RoleManager.ROLE_SMS, pkg, RoleManager.MANAGE_HOLDERS_FLAG_DONT_KILL_APP,
                    context.getUser(), AsyncTask.THREAD_POOL_EXECUTOR, result::complete);
            assertTrue(result.get(5, TimeUnit.SECONDS));
        });
    }

    private <T> T executeWithShellPermissionIdentity(Callable<T> callable) throws Exception {
        if (sHasShellPermissionIdentity) {
            return callable.call();
        }
        UiAutomation uiAutomation = getInstrumentation().getUiAutomation(
                UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        uiAutomation.adoptShellPermissionIdentity();
        try {
            sHasShellPermissionIdentity = true;
            return callable.call();
        } finally {
            uiAutomation.dropShellPermissionIdentity();
            sHasShellPermissionIdentity = false;
        }
    }

    private void executeWithShellPermissionIdentity(RunnableWithException runnable)
            throws Exception {
        executeWithShellPermissionIdentity(() -> {
            runnable.run();
            return null;
        });
    }

    private interface RunnableWithException {
        void run() throws Exception;
    }

    private void assertCanAccessSms(String pkg) throws Exception {
        CompletableFuture<Bundle> callbackResult = new CompletableFuture<>();
        mContext.startActivity(new Intent()
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .setComponent(new ComponentName(pkg, pkg + ".MainActivity"))
                .putExtra("callback", new RemoteCallback(callbackResult::complete)));

        Bundle bundle = callbackResult.get(20, TimeUnit.SECONDS);

        assertThat(bundle.getString("class"), startsWith(pkg));
        assertThat(bundle.getString("exceptionMessage"), anyOf(equalTo(null), emptyString()));
        assertThat(bundle.getInt("queryCount"), greaterThan(0));
    }

    private void init() {
        mSendReceiver.reset();
        mDeliveryReceiver.reset();
        mDataSmsReceiver.reset();
        mSmsDeliverReceiver.reset();
        mSmsReceivedReceiver.reset();
        mSmsRetrieverReceiver.reset();
        mReceivedDataSms = false;
        sMessageId = 0L;
        mSentIntent = PendingIntent.getBroadcast(mContext, 0, mSendIntent,
                PendingIntent.FLAG_ONE_SHOT);
        mDeliveredIntent = PendingIntent.getBroadcast(mContext, 0, mDeliveryIntent,
                PendingIntent.FLAG_ONE_SHOT);
    }

    private void setupBroadcastReceivers() {
        mSendIntent = new Intent(SMS_SEND_ACTION);
        mDeliveryIntent = new Intent(SMS_DELIVERY_ACTION);

        IntentFilter sendIntentFilter = new IntentFilter(SMS_SEND_ACTION);
        IntentFilter deliveryIntentFilter = new IntentFilter(SMS_DELIVERY_ACTION);
        IntentFilter dataSmsReceivedIntentFilter = new IntentFilter(DATA_SMS_RECEIVED_ACTION);
        IntentFilter smsDeliverIntentFilter = new IntentFilter(SMS_DELIVER_DEFAULT_APP_ACTION);
        IntentFilter smsReceivedIntentFilter =
                new IntentFilter(Telephony.Sms.Intents.SMS_RECEIVED_ACTION);
        IntentFilter smsRetrieverIntentFilter = new IntentFilter(SMS_RETRIEVER_ACTION);
        dataSmsReceivedIntentFilter.addDataScheme("sms");
        dataSmsReceivedIntentFilter.addDataAuthority("localhost", "19989");

        mSendReceiver = new SmsBroadcastReceiver(SMS_SEND_ACTION);
        mDeliveryReceiver = new SmsBroadcastReceiver(SMS_DELIVERY_ACTION);
        mDataSmsReceiver = new SmsBroadcastReceiver(DATA_SMS_RECEIVED_ACTION);
        mSmsDeliverReceiver = new SmsBroadcastReceiver(SMS_DELIVER_DEFAULT_APP_ACTION);
        mSmsReceivedReceiver = new SmsBroadcastReceiver(Telephony.Sms.Intents.SMS_RECEIVED_ACTION);
        mSmsRetrieverReceiver = new SmsBroadcastReceiver(SMS_RETRIEVER_ACTION);

        mContext.registerReceiver(mSendReceiver, sendIntentFilter);
        mContext.registerReceiver(mDeliveryReceiver, deliveryIntentFilter);
        mContext.registerReceiver(mDataSmsReceiver, dataSmsReceivedIntentFilter);
        mContext.registerReceiver(mSmsDeliverReceiver, smsDeliverIntentFilter);
        mContext.registerReceiver(mSmsReceivedReceiver, smsReceivedIntentFilter);
        mContext.registerReceiver(mSmsRetrieverReceiver, smsRetrieverIntentFilter);
    }

    /**
     * Returns the number of parts sent in the message. If Multi-part SMS is not supported,
     * returns 0.
     */
    private int sendMultipartTextMessageIfSupported(String mccmnc, boolean addMessageId) {
        int numPartsSent = 0;
        if (!CarrierCapability.UNSUPPORT_MULTIPART_SMS_MESSAGES.contains(mccmnc)) {
            init();
            ArrayList<String> parts = divideMessage(LONG_TEXT);
            numPartsSent = parts.size();
            ArrayList<PendingIntent> sentIntents = new ArrayList<PendingIntent>();
            ArrayList<PendingIntent> deliveryIntents = new ArrayList<PendingIntent>();
            for (int i = 0; i < numPartsSent; i++) {
                sentIntents.add(PendingIntent.getBroadcast(mContext, 0, mSendIntent, 0));
                deliveryIntents.add(PendingIntent.getBroadcast(mContext, 0, mDeliveryIntent, 0));
            }
            sendMultiPartTextMessage(mDestAddr, parts, sentIntents, deliveryIntents, addMessageId);
        }
        return numPartsSent;
    }

    private boolean sendDataMessageIfSupported(String mccmnc) {
        if (!CarrierCapability.UNSUPPORT_DATA_SMS_MESSAGES.contains(mccmnc)) {
            byte[] data = mText.getBytes();
            short port = 19989;

            init();
            sendDataMessage(mDestAddr, port, data, mSentIntent, mDeliveredIntent);
            return true;
        }
        return false;
    }

    @Test
    public void testGetDefault() {
        assertNotNull(getSmsManager());
    }

    @Test
    public void testGetSmscAddress() {
        try {
            getSmsManager().getSmscAddress();
            fail("SmsManager.getSmscAddress() should throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.READ_PRIVILEGED_PHONE_STATE");
        try {
            getSmsManager().getSmscAddress();
        } catch (SecurityException se) {
            fail("Caller with READ_PRIVILEGED_PHONE_STATE should be able to call API");
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testSetSmscAddress() {
        try {
            getSmsManager().setSmscAddress("fake smsc");
            fail("SmsManager.setSmscAddress() should throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.MODIFY_PHONE_STATE");
        try {
            getSmsManager().setSmscAddress("fake smsc");
        } catch (SecurityException se) {
            fail("Caller with MODIFY_PHONE_STATE should be able to call API");
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testGetPremiumSmsConsent() {
        try {
            getSmsManager().getPremiumSmsConsent("fake package name");
            fail("SmsManager.getPremiumSmsConsent() should throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.READ_PRIVILEGED_PHONE_STATE");
        try {
            getSmsManager().getPremiumSmsConsent("fake package name");
            fail("Caller with permission but only phone/system uid is allowed");
        } catch (SecurityException se) {
            // expected
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testSetPremiumSmsConsent() {
        try {
            getSmsManager().setPremiumSmsConsent("fake package name", 0);
            fail("SmsManager.setPremiumSmsConsent() should throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.MODIFY_PHONE_STATE");
        try {
            getSmsManager().setPremiumSmsConsent("fake package name", 0);
            fail("Caller with permission but only phone/system uid is allowed");
        } catch (SecurityException se) {
            // expected
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testGetSmsCapacityOnIcc() {
        try {
            getSmsManager().getSmsCapacityOnIcc();
            fail("Caller without READ_PRIVILEGED_PHONE_STATE should NOT be able to call API");
        } catch (SecurityException se) {
            // all good
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.READ_PRIVILEGED_PHONE_STATE");
        try {
            getSmsManager().getSmsCapacityOnIcc();
        } catch (SecurityException se) {
            fail("Caller with READ_PRIVILEGED_PHONE_STATE should be able to call API");
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testDisableCellBroadcastRange() {
        try {
            int ranType = SmsCbMessage.MESSAGE_FORMAT_3GPP;
            executeWithShellPermissionIdentity(() -> {
                getSmsManager().disableCellBroadcastRange(
                        CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT,
                        ranType);
            });
        } catch (Exception e) {
            // expected
        }
    }

    @Test
    public void testEnableCellBroadcastRange() {
        try {
            int ranType = SmsCbMessage.MESSAGE_FORMAT_3GPP;
            executeWithShellPermissionIdentity(() -> {
                getSmsManager().enableCellBroadcastRange(
                        CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT,
                        ranType);
            });
        } catch (Exception e) {
            // expected
        }
    }

    protected ArrayList<String> divideMessage(String text) {
        return getSmsManager().divideMessage(text);
    }

    private android.telephony.SmsManager getSmsManager() {
        return android.telephony.SmsManager.getDefault();
    }

    protected void sendMultiPartTextMessage(String destAddr, ArrayList<String> parts,
            ArrayList<PendingIntent> sentIntents, ArrayList<PendingIntent> deliveryIntents,
            boolean addMessageId) {
        if (addMessageId) {
            long fakeMessageId = 1278;
            getSmsManager().sendMultipartTextMessage(destAddr, null, parts, sentIntents,
                    deliveryIntents, fakeMessageId);
        } else if (mContext.getOpPackageName() != null) {
            getSmsManager().sendMultipartTextMessage(destAddr, null, parts, sentIntents,
                    deliveryIntents, mContext.getOpPackageName(), mContext.getAttributionTag());
        } else {
            getSmsManager().sendMultipartTextMessage(destAddr, null, parts, sentIntents,
                    deliveryIntents);
        }
    }

    protected void sendDataMessage(String destAddr,short port, byte[] data, PendingIntent sentIntent, PendingIntent deliveredIntent) {
        getSmsManager().sendDataMessage(destAddr, null, port, data, sentIntent, deliveredIntent);
    }

    protected void sendTextMessage(String destAddr, String text, PendingIntent sentIntent,
            PendingIntent deliveredIntent) {
        getSmsManager().sendTextMessage(destAddr, null, text, sentIntent, deliveredIntent);
    }

    protected void sendTextMessageWithMessageId(String destAddr, String text,
            PendingIntent sentIntent, PendingIntent deliveredIntent, long messageId) {
        getSmsManager().sendTextMessage(destAddr, null, text, sentIntent, deliveredIntent,
                messageId);
    }

    private void blockNumber(String number) {
        mBlockedNumberUri = insertBlockedNumber(mContext, number);
        if (mBlockedNumberUri == null) {
            fail("Failed to insert into blocked number provider.");
        }
    }

    private void unblockNumber(Uri uri) {
        deleteBlockedNumber(mContext, uri);
    }

    private void setDefaultSmsApp(boolean setToSmsApp)
            throws Exception {
        String command = String.format(
                "appops set --user 0 %s WRITE_SMS %s",
                mContext.getPackageName(),
                setToSmsApp ? "allow" : "default");
        assertTrue("Setting default SMS app failed : " + setToSmsApp,
                executeShellCommand(command).isEmpty());
        mTestAppSetAsDefaultSmsApp = setToSmsApp;
    }

    private String executeShellCommand(String command)
            throws IOException {
        ParcelFileDescriptor pfd =
                getInstrumentation().getUiAutomation().executeShellCommand(command);
        BufferedReader br = null;
        try (InputStream in = new FileInputStream(pfd.getFileDescriptor());) {
            br = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8));
            String str;
            StringBuilder out = new StringBuilder();
            while ((str = br.readLine()) != null) {
                out.append(str);
            }
            return out.toString();
        } finally {
            if (br != null) {
                br.close();
            }
        }
    }

    private static class SmsBroadcastReceiver extends BroadcastReceiver {
        private int mCalls;
        private int mExpectedCalls;
        private String mAction;
        private Object mLock;

        SmsBroadcastReceiver(String action) {
            mAction = action;
            reset();
            mLock = new Object();
        }

        void reset() {
            mExpectedCalls = Integer.MAX_VALUE;
            mCalls = 0;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if(mAction.equals(DATA_SMS_RECEIVED_ACTION)){
                StringBuilder sb = new StringBuilder();
                Bundle bundle = intent.getExtras();
                if (bundle != null) {
                    Object[] obj = (Object[]) bundle.get("pdus");
                    String format = bundle.getString("format");
                    SmsMessage[] message = new SmsMessage[obj.length];
                    for (int i = 0; i < obj.length; i++) {
                        message[i] = SmsMessage.createFromPdu((byte[]) obj[i], format);
                    }

                    for (SmsMessage currentMessage : message) {
                        byte[] binaryContent = currentMessage.getUserData();
                        String readableContent = new String(binaryContent);
                        sb.append(readableContent);
                    }
                }
                mReceivedDataSms = true;
                mReceivedText=sb.toString();
            }
            if (mAction.equals(Telephony.Sms.Intents.SMS_RECEIVED_ACTION)) {
                sMessageId = intent.getLongExtra("messageId", 0L);
            }
            Log.i(TAG, "onReceive " + intent.getAction());
            if (intent.getAction().equals(mAction)) {
                synchronized (mLock) {
                    mCalls += 1;
                    mLock.notify();
                }
            }
        }

        private boolean verifyNoCalls(long timeout) throws InterruptedException {
            synchronized(mLock) {
                mLock.wait(timeout);
                return mCalls == 0;
            }
        }

        public boolean waitForCalls(int expectedCalls, long timeout) throws InterruptedException {
            synchronized(mLock) {
                mExpectedCalls = expectedCalls;
                long startTime = SystemClock.elapsedRealtime();

                while (mCalls < mExpectedCalls) {
                    long waitTime = timeout - (SystemClock.elapsedRealtime() - startTime);
                    if (waitTime > 0) {
                        mLock.wait(waitTime);
                    } else {
                        return false;  // timed out
                    }
                }
                return true;  // success
            }
        }
    }
}
