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

package com.android.mms.service;

import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE;
import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE_SEND_REQ;

import android.annotation.Nullable;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.sqlite.SQLiteException;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.provider.Settings;
import android.provider.Telephony;
import android.security.NetworkSecurityPolicy;
import android.service.carrier.CarrierMessagingService;
import android.telephony.SmsManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.data.ApnSetting;
import android.text.TextUtils;
import android.util.EventLog;
import android.util.SparseArray;

import com.android.internal.telephony.IMms;

import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.DeliveryInd;
import com.google.android.mms.pdu.GenericPdu;
import com.google.android.mms.pdu.NotificationInd;
import com.google.android.mms.pdu.PduParser;
import com.google.android.mms.pdu.PduPersister;
import com.google.android.mms.pdu.ReadOrigInd;
import com.google.android.mms.pdu.RetrieveConf;
import com.google.android.mms.pdu.SendReq;
import com.google.android.mms.util.SqliteWrapper;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * System service to process MMS API requests
 */
public class MmsService extends Service implements MmsRequest.RequestManager {
    public static final int QUEUE_INDEX_SEND = 0;
    public static final int QUEUE_INDEX_DOWNLOAD = 1;

    private static final String SHARED_PREFERENCES_NAME = "mmspref";
    private static final String PREF_AUTO_PERSISTING = "autopersisting";

    // Maximum time to spend waiting to read data from a content provider before failing with error.
    private static final int TASK_TIMEOUT_MS = 30 * 1000;
    // Maximum size of MMS service supports - used on occassions when MMS messages are processed
    // in a carrier independent manner (for example for imports and drafts) and the carrier
    // specific size limit should not be used (as it could be lower on some carriers).
    private static final int MAX_MMS_FILE_SIZE = 8 * 1024 * 1024;

    // The default number of threads allowed to run MMS requests in each queue
    public static final int THREAD_POOL_SIZE = 4;

    /** Represents the received SMS message for importing. */
    public static final int SMS_TYPE_INCOMING = 0;
    /** Represents the sent SMS message for importing. */
    public static final int SMS_TYPE_OUTGOING = 1;
    /** Message status property: whether the message has been seen. */
    public static final String MESSAGE_STATUS_SEEN = "seen";
    /** Message status property: whether the message has been read. */
    public static final String MESSAGE_STATUS_READ = "read";

    // Pending requests that are waiting for the SIM to be available
    // If a different SIM is currently used by previous requests, the following
    // requests will stay in this queue until that SIM finishes its current requests in
    // RequestQueue.
    // Requests are not reordered. So, e.g. if current SIM is SIM1, a request for SIM2 will be
    // blocked in the queue. And a later request for SIM1 will be appended to the queue, ordered
    // after the request for SIM2, instead of being put into the running queue.
    // TODO: persist this in case MmsService crashes
    private final Queue<MmsRequest> mPendingSimRequestQueue = new ArrayDeque<>();

    // Thread pool for transferring PDU with MMS apps
    private final ExecutorService mPduTransferExecutor = Executors.newCachedThreadPool();

    // A cache of MmsNetworkManager for SIMs
    private final SparseArray<MmsNetworkManager> mNetworkManagerCache = new SparseArray<>();

    // The default TelephonyManager and a cache of TelephonyManagers for individual subscriptions
    private TelephonyManager mDefaultTelephonyManager;
    private final SparseArray<TelephonyManager> mTelephonyManagerCache = new SparseArray<>();

    // The current SIM ID for the running requests. Only one SIM can send/download MMS at a time.
    private int mCurrentSubId;
    // The current running MmsRequest count.
    private int mRunningRequestCount;

    // Running request queues, one thread pool per queue
    // 0: send queue
    // 1: download queue
    private final ExecutorService[] mRunningRequestExecutors = new ExecutorService[2];

    private MmsNetworkManager getNetworkManager(int subId) {
        synchronized (mNetworkManagerCache) {
            MmsNetworkManager manager = mNetworkManagerCache.get(subId);
            if (manager == null) {
                manager = new MmsNetworkManager(this, subId);
                mNetworkManagerCache.put(subId, manager);
            }
            return manager;
        }
    }

    private TelephonyManager getTelephonyManager(int subId) {
        synchronized (mTelephonyManagerCache) {
            if (mDefaultTelephonyManager == null) {
                mDefaultTelephonyManager = (TelephonyManager) this.getSystemService(
                        Context.TELEPHONY_SERVICE);
            }

            TelephonyManager subSpecificTelephonyManager = mTelephonyManagerCache.get(subId);
            if (subSpecificTelephonyManager == null) {
                subSpecificTelephonyManager = mDefaultTelephonyManager.createForSubscriptionId(
                        subId);
                mTelephonyManagerCache.put(subId, subSpecificTelephonyManager);
            }
            return subSpecificTelephonyManager;
        }
    }

    private void enforceSystemUid() {
        if (Binder.getCallingUid() != Process.SYSTEM_UID) {
            throw new SecurityException("Only system can call this service");
        }
    }

    @Nullable
    private String getCarrierMessagingServicePackageIfExists(int subId) {
        Intent intent = new Intent(CarrierMessagingService.SERVICE_INTERFACE);
        TelephonyManager telephonyManager = getTelephonyManager(subId);
        List<String> carrierPackages = telephonyManager.getCarrierPackageNamesForIntent(intent);

        if (carrierPackages == null || carrierPackages.size() != 1) {
            return null;
        } else {
            return carrierPackages.get(0);
        }
    }

    private IMms.Stub mStub = new IMms.Stub() {
        @Override
        public void sendMessage(int subId, String callingPkg, Uri contentUri,
                String locationUrl, Bundle configOverrides, PendingIntent sentIntent,
                long messageId) {
            LogUtil.d("sendMessage messageId: " + messageId);
            enforceSystemUid();

            // Make sure the subId is correct
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                LogUtil.e("Invalid subId " + subId);
                sendErrorInPendingIntent(sentIntent);
                return;
            }
            if (subId == SubscriptionManager.DEFAULT_SUBSCRIPTION_ID) {
                subId = SubscriptionManager.getDefaultSmsSubscriptionId();
            }

            // Make sure the subId is active
            if (!isActiveSubId(subId)) {
                sendErrorInPendingIntent(sentIntent);
                return;
            }

            final SendRequest request = new SendRequest(MmsService.this, subId, contentUri,
                    locationUrl, sentIntent, callingPkg, configOverrides, MmsService.this,
                    messageId);

            final String carrierMessagingServicePackage =
                    getCarrierMessagingServicePackageIfExists(subId);

            if (carrierMessagingServicePackage != null) {
                LogUtil.d(request.toString(), "sending message by carrier app");
                request.trySendingByCarrierApp(MmsService.this, carrierMessagingServicePackage);
                return;
            }

            // Make sure subId has MMS data. We intentionally do this after attempting to send via a
            // carrier messaging service as the carrier messaging service may want to handle this in
            // a different way and may not be restricted by whether data is enabled for an APN on a
            // given subscription.
            if (!getTelephonyManager(subId).isDataEnabledForApn(ApnSetting.TYPE_MMS)) {
                // ENABLE_MMS_DATA_REQUEST_REASON_OUTGOING_MMS is set for only SendReq case, since
                // AcknowledgeInd and NotifyRespInd are parts of downloading sequence.
                // TODO: Should consider ReadRecInd(Read Report)?
                sendSettingsIntentForFailedMms(!isRawPduSendReq(contentUri), subId);
                sendErrorInPendingIntent(sentIntent);
                return;
            }

            addSimRequest(request);
        }

        @Override
        public void downloadMessage(int subId, String callingPkg, String locationUrl,
                Uri contentUri, Bundle configOverrides,
                PendingIntent downloadedIntent, long messageId) {
            // If the subId is no longer active it could be caused by an MVNO using multiple
            // subIds, so we should try to download anyway.
            // TODO: Fail fast when downloading will fail (i.e. SIM swapped)
            LogUtil.d("downloadMessage: " + MmsHttpClient.redactUrlForNonVerbose(locationUrl) +
                    ", messageId: " + messageId);

            enforceSystemUid();

            // Make sure the subId is correct
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                LogUtil.e("Invalid subId " + subId);
                sendErrorInPendingIntent(downloadedIntent);
                return;
            }
            if (subId == SubscriptionManager.DEFAULT_SUBSCRIPTION_ID) {
                subId = SubscriptionManager.getDefaultSmsSubscriptionId();
            }

            if (!isActiveSubId(subId)) {
                List<SubscriptionInfo> activeSubList = getActiveSubscriptionsInGroup(subId);
                if (activeSubList.isEmpty()) {
                    sendErrorInPendingIntent(downloadedIntent);
                    return;
                }

                subId = activeSubList.get(0).getSubscriptionId();
                int defaultSmsSubId = SubscriptionManager.getDefaultSmsSubscriptionId();
                // If we have default sms subscription, prefer to use that. Otherwise, use first
                // subscription
                for (SubscriptionInfo subInfo : activeSubList) {
                    if (subInfo.getSubscriptionId() == defaultSmsSubId) {
                        subId = subInfo.getSubscriptionId();
                    }
                }
            }

            final DownloadRequest request = new DownloadRequest(MmsService.this, subId, locationUrl,
                    contentUri, downloadedIntent, callingPkg, configOverrides, MmsService.this,
                    messageId);

            final String carrierMessagingServicePackage =
                    getCarrierMessagingServicePackageIfExists(subId);

            if (carrierMessagingServicePackage != null) {
                LogUtil.d(request.toString(), "downloading message by carrier app");
                request.tryDownloadingByCarrierApp(MmsService.this, carrierMessagingServicePackage);
                return;
            }

            // Make sure subId has MMS data
            if (!getTelephonyManager(subId).isDataEnabledForApn(ApnSetting.TYPE_MMS)) {
                sendSettingsIntentForFailedMms(/*isIncoming=*/ true, subId);
                sendErrorInPendingIntent(downloadedIntent);
                return;
            }

            addSimRequest(request);
        }

        private List<SubscriptionInfo> getActiveSubscriptionsInGroup(int subId) {
            SubscriptionManager subManager =
                    (SubscriptionManager) getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);

            if (subManager == null) {
                return Collections.emptyList();
            }

            List<SubscriptionInfo> subList = subManager.getAvailableSubscriptionInfoList();

            if (subList == null) {
                return Collections.emptyList();
            }

            SubscriptionInfo subscriptionInfo = null;
            for (SubscriptionInfo subInfo : subList) {
                if (subInfo.getSubscriptionId() == subId) {
                    subscriptionInfo = subInfo;
                    break;
                }
            }

            if (subscriptionInfo == null) {
                return Collections.emptyList();
            }

            if (subscriptionInfo.getGroupUuid() == null) {
                return Collections.emptyList();
            }

            List<SubscriptionInfo> subscriptionInGroupList =
                    subManager.getSubscriptionsInGroup(subscriptionInfo.getGroupUuid());

            // the list is sorted by isOpportunistic and isOpportunistic == false will have higher
            // priority
            return subscriptionInGroupList.stream()
                    .filter(info ->
                            info.getSimSlotIndex() != SubscriptionManager.INVALID_SIM_SLOT_INDEX)
                    .sorted(Comparator.comparing(SubscriptionInfo::isOpportunistic))
                    .collect(Collectors.toList());
        }

        @Override
        public Uri importTextMessage(String callingPkg, String address, int type, String text,
                long timestampMillis, boolean seen, boolean read) {
            LogUtil.d("importTextMessage");
            enforceSystemUid();
            return importSms(address, type, text, timestampMillis, seen, read, callingPkg);
        }

        @Override
        public Uri importMultimediaMessage(String callingPkg, Uri contentUri,
                String messageId, long timestampSecs, boolean seen, boolean read) {
            LogUtil.d("importMultimediaMessage");
            enforceSystemUid();
            return importMms(contentUri, messageId, timestampSecs, seen, read, callingPkg);
        }

        @Override
        public boolean deleteStoredMessage(String callingPkg, Uri messageUri)
                throws RemoteException {
            LogUtil.d("deleteStoredMessage " + messageUri);
            enforceSystemUid();
            if (!isSmsMmsContentUri(messageUri)) {
                LogUtil.e("deleteStoredMessage: invalid message URI: " + messageUri.toString());
                return false;
            }
            // Clear the calling identity and query the database using the phone user id
            // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
            // between the calling uid and the package uid
            final long identity = Binder.clearCallingIdentity();
            try {
                if (getContentResolver().delete(
                        messageUri, null/*where*/, null/*selectionArgs*/) != 1) {
                    LogUtil.e("deleteStoredMessage: failed to delete");
                    return false;
                }
            } catch (SQLiteException e) {
                LogUtil.e("deleteStoredMessage: failed to delete", e);
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
            return true;
        }

        @Override
        public boolean deleteStoredConversation(String callingPkg, long conversationId)
                throws RemoteException {
            LogUtil.d("deleteStoredConversation " + conversationId);
            enforceSystemUid();
            if (conversationId == -1) {
                LogUtil.e("deleteStoredConversation: invalid thread id");
                return false;
            }
            final Uri uri = ContentUris.withAppendedId(
                    Telephony.Threads.CONTENT_URI, conversationId);
            // Clear the calling identity and query the database using the phone user id
            // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
            // between the calling uid and the package uid
            final long identity = Binder.clearCallingIdentity();
            try {
                if (getContentResolver().delete(uri, null, null) != 1) {
                    LogUtil.e("deleteStoredConversation: failed to delete");
                    return false;
                }
            } catch (SQLiteException e) {
                LogUtil.e("deleteStoredConversation: failed to delete", e);
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
            return true;
        }

        @Override
        public boolean updateStoredMessageStatus(String callingPkg, Uri messageUri,
                ContentValues statusValues) throws RemoteException {
            LogUtil.d("updateStoredMessageStatus " + messageUri);
            enforceSystemUid();
            return updateMessageStatus(messageUri, statusValues);
        }

        @Override
        public boolean archiveStoredConversation(String callingPkg, long conversationId,
                boolean archived) throws RemoteException {
            LogUtil.d("archiveStoredConversation " + conversationId + " " + archived);
            if (Binder.getCallingUid() != Process.SYSTEM_UID) {
                EventLog.writeEvent(0x534e4554, "180419673", Binder.getCallingUid(), "");
            }
            enforceSystemUid();
            if (conversationId == -1) {
                LogUtil.e("archiveStoredConversation: invalid thread id");
                return false;
            }
            return archiveConversation(conversationId, archived);
        }

        @Override
        public Uri addTextMessageDraft(String callingPkg, String address, String text)
                throws RemoteException {
            LogUtil.d("addTextMessageDraft");
            enforceSystemUid();
            return addSmsDraft(address, text, callingPkg);
        }

        @Override
        public Uri addMultimediaMessageDraft(String callingPkg, Uri contentUri)
                throws RemoteException {
            LogUtil.d("addMultimediaMessageDraft");
            enforceSystemUid();
            return addMmsDraft(contentUri, callingPkg);
        }

        @Override
        public void sendStoredMessage(int subId, String callingPkg, Uri messageUri,
                Bundle configOverrides, PendingIntent sentIntent) throws RemoteException {
            throw new UnsupportedOperationException();
        }

        @Override
        public void setAutoPersisting(String callingPkg, boolean enabled) throws RemoteException {
            LogUtil.d("setAutoPersisting " + enabled);
            enforceSystemUid();
            final SharedPreferences preferences = getSharedPreferences(
                    SHARED_PREFERENCES_NAME, MODE_PRIVATE);
            final SharedPreferences.Editor editor = preferences.edit();
            editor.putBoolean(PREF_AUTO_PERSISTING, enabled);
            editor.apply();
        }

        @Override
        public boolean getAutoPersisting() throws RemoteException {
            LogUtil.d("getAutoPersisting");
            return getAutoPersistingPref();
        }

        /*
         * @return true if the subId is active.
         */
        private boolean isActiveSubId(int subId) {
            return ((SubscriptionManager) getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE))
                .isActiveSubscriptionId(subId);
        }

        /*
         * Calls the pending intent with <code>MMS_ERROR_NO_DATA_NETWORK</code>.
         */
        private void sendErrorInPendingIntent(@Nullable PendingIntent intent) {
            if (intent != null) {
                try {
                    intent.send(SmsManager.MMS_ERROR_NO_DATA_NETWORK);
                } catch (PendingIntent.CanceledException ex) {
                }
            }
        }

        private boolean isRawPduSendReq(Uri contentUri) {
            // X-Mms-Message-Type is at the beginning of the message headers always. 1st byte is
            // MMS-filed-name and 2nd byte is MMS-value for X-Mms-Message-Type field.
            // See OMA-TS-MMS_ENC-V1_3-20110913-A, 7. Binary Encoding of ProtocolData Units
            byte[] pduData = new byte[2];
            int bytesRead = readPduBytesFromContentUri(contentUri, pduData);

            // Return true for MESSAGE_TYPE_SEND_REQ only. Otherwise false even wrong PDU case.
            if (bytesRead == 2
                    && (pduData[0] & 0xFF) == MESSAGE_TYPE
                    && (pduData[1] & 0xFF) == MESSAGE_TYPE_SEND_REQ) {
                return true;
            }
            return false;
        }
    };

    @Override
    public void addSimRequest(MmsRequest request) {
        if (request == null) {
            LogUtil.e("Add running or pending: empty request");
            return;
        }
        LogUtil.d("Current running=" + mRunningRequestCount + ", "
                + "current subId=" + mCurrentSubId + ", "
                + "pending=" + mPendingSimRequestQueue.size());
        synchronized (this) {
            if (mPendingSimRequestQueue.size() > 0 ||
                    (mRunningRequestCount > 0 && request.getSubId() != mCurrentSubId)) {
                LogUtil.d("Add request to pending queue."
                        + " Request subId=" + request.getSubId() + ","
                        + " current subId=" + mCurrentSubId);
                mPendingSimRequestQueue.add(request);
                if (mRunningRequestCount <= 0) {
                    LogUtil.e("Nothing's running but queue's not empty");
                    // Nothing is running but we are accumulating on pending queue.
                    // This should not happen. But just in case...
                    movePendingSimRequestsToRunningSynchronized();
                }
            } else {
                addToRunningRequestQueueSynchronized(request);
            }
        }
    }

    private void sendSettingsIntentForFailedMms(boolean isIncoming, int subId) {
        LogUtil.w("Subscription with id: " + subId
                + " cannot " + (isIncoming ? "download" : "send")
                + " MMS, data connection is not available");
        Intent intent = new Intent(Settings.ACTION_ENABLE_MMS_DATA_REQUEST);

        intent.putExtra(Settings.EXTRA_ENABLE_MMS_DATA_REQUEST_REASON,
                isIncoming ? Settings.ENABLE_MMS_DATA_REQUEST_REASON_INCOMING_MMS
                        : Settings.ENABLE_MMS_DATA_REQUEST_REASON_OUTGOING_MMS);

        intent.putExtra(Settings.EXTRA_SUB_ID, subId);

        this.sendBroadcastAsUser(intent, UserHandle.SYSTEM,
                android.Manifest.permission.NETWORK_SETTINGS);
    }

    private void addToRunningRequestQueueSynchronized(final MmsRequest request) {
        LogUtil.d("Add request to running queue for subId " + request.getSubId());
        // Update current state of running requests
        final int queue = request.getQueueType();
        if (queue < 0 || queue >= mRunningRequestExecutors.length) {
            LogUtil.e("Invalid request queue index for running request");
            return;
        }
        mRunningRequestCount++;
        mCurrentSubId = request.getSubId();
        // Send to the corresponding request queue for execution
        mRunningRequestExecutors[queue].execute(new Runnable() {
            @Override
            public void run() {
                try {
                    request.execute(MmsService.this, getNetworkManager(request.getSubId()));
                } finally {
                    synchronized (MmsService.this) {
                        mRunningRequestCount--;
                        if (mRunningRequestCount <= 0) {
                            movePendingSimRequestsToRunningSynchronized();
                        }
                    }
                }
            }
        });
    }

    private void movePendingSimRequestsToRunningSynchronized() {
        LogUtil.d("Schedule requests pending on SIM");
        mCurrentSubId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        while (mPendingSimRequestQueue.size() > 0) {
            final MmsRequest request = mPendingSimRequestQueue.peek();
            if (request != null) {
                if (!SubscriptionManager.isValidSubscriptionId(mCurrentSubId)
                        || mCurrentSubId == request.getSubId()) {
                    // First or subsequent requests with same SIM ID
                    mPendingSimRequestQueue.remove();
                    addToRunningRequestQueueSynchronized(request);
                } else {
                    // Stop if we see a different SIM ID
                    break;
                }
            } else {
                LogUtil.e("Schedule pending: found empty request");
                mPendingSimRequestQueue.remove();
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mStub;
    }

    public final IBinder asBinder() {
        return mStub;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        LogUtil.d("onCreate");
        // Load mms_config
        MmsConfigManager.getInstance().init(this);

        NetworkSecurityPolicy.getInstance().setCleartextTrafficPermitted(true);

        // Initialize running request state
        for (int i = 0; i < mRunningRequestExecutors.length; i++) {
            mRunningRequestExecutors[i] = Executors.newFixedThreadPool(THREAD_POOL_SIZE);
        }
        synchronized (this) {
            mCurrentSubId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
            mRunningRequestCount = 0;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        LogUtil.d("onDestroy");
        for (ExecutorService executor : mRunningRequestExecutors) {
            executor.shutdown();
        }
    }

    private Uri importSms(String address, int type, String text, long timestampMillis,
            boolean seen, boolean read, String creator) {
        Uri insertUri = null;
        switch (type) {
            case SMS_TYPE_INCOMING:
                insertUri = Telephony.Sms.Inbox.CONTENT_URI;

                break;
            case SMS_TYPE_OUTGOING:
                insertUri = Telephony.Sms.Sent.CONTENT_URI;
                break;
        }
        if (insertUri == null) {
            LogUtil.e("importTextMessage: invalid message type for importing: " + type);
            return null;
        }
        final ContentValues values = new ContentValues(6);
        values.put(Telephony.Sms.ADDRESS, address);
        values.put(Telephony.Sms.DATE, timestampMillis);
        values.put(Telephony.Sms.SEEN, seen ? 1 : 0);
        values.put(Telephony.Sms.READ, read ? 1 : 0);
        values.put(Telephony.Sms.BODY, text);
        if (!TextUtils.isEmpty(creator)) {
            values.put(Telephony.Mms.CREATOR, creator);
        }
        // Clear the calling identity and query the database using the phone user id
        // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
        // between the calling uid and the package uid
        final long identity = Binder.clearCallingIdentity();
        try {
            return getContentResolver().insert(insertUri, values);
        } catch (SQLiteException e) {
            LogUtil.e("importTextMessage: failed to persist imported text message", e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return null;
    }

    private Uri importMms(Uri contentUri, String messageId, long timestampSecs,
            boolean seen, boolean read, String creator) {
        byte[] pduData = readPduFromContentUri(contentUri, MAX_MMS_FILE_SIZE);
        if (pduData == null || pduData.length < 1) {
            LogUtil.e("importMessage: empty PDU");
            return null;
        }
        // Clear the calling identity and query the database using the phone user id
        // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
        // between the calling uid and the package uid
        final long identity = Binder.clearCallingIdentity();
        try {
            final GenericPdu pdu = parsePduForAnyCarrier(pduData);
            if (pdu == null) {
                LogUtil.e("importMessage: can't parse input PDU");
                return null;
            }
            Uri insertUri = null;
            if (pdu instanceof SendReq) {
                insertUri = Telephony.Mms.Sent.CONTENT_URI;
            } else if (pdu instanceof RetrieveConf ||
                    pdu instanceof NotificationInd ||
                    pdu instanceof DeliveryInd ||
                    pdu instanceof ReadOrigInd) {
                insertUri = Telephony.Mms.Inbox.CONTENT_URI;
            }
            if (insertUri == null) {
                LogUtil.e("importMessage; invalid MMS type: " + pdu.getClass().getCanonicalName());
                return null;
            }
            final PduPersister persister = PduPersister.getPduPersister(this);
            final Uri uri = persister.persist(
                    pdu,
                    insertUri,
                    true/*createThreadId*/,
                    true/*groupMmsEnabled*/,
                    null/*preOpenedFiles*/);
            if (uri == null) {
                LogUtil.e("importMessage: failed to persist message");
                return null;
            }
            final ContentValues values = new ContentValues(5);
            if (!TextUtils.isEmpty(messageId)) {
                values.put(Telephony.Mms.MESSAGE_ID, messageId);
            }
            if (timestampSecs != -1) {
                values.put(Telephony.Mms.DATE, timestampSecs);
            }
            values.put(Telephony.Mms.READ, seen ? 1 : 0);
            values.put(Telephony.Mms.SEEN, read ? 1 : 0);
            if (!TextUtils.isEmpty(creator)) {
                values.put(Telephony.Mms.CREATOR, creator);
            }
            if (SqliteWrapper.update(this, getContentResolver(), uri, values,
                    null/*where*/, null/*selectionArg*/) != 1) {
                LogUtil.e("importMessage: failed to update message");
            }
            return uri;
        } catch (RuntimeException e) {
            LogUtil.e("importMessage: failed to parse input PDU", e);
        } catch (MmsException e) {
            LogUtil.e("importMessage: failed to persist message", e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return null;
    }

    private static boolean isSmsMmsContentUri(Uri uri) {
        final String uriString = uri.toString();
        if (!uriString.startsWith("content://sms/") && !uriString.startsWith("content://mms/")) {
            return false;
        }
        if (ContentUris.parseId(uri) == -1) {
            return false;
        }
        return true;
    }

    private boolean updateMessageStatus(Uri messageUri, ContentValues statusValues) {
        if (!isSmsMmsContentUri(messageUri)) {
            LogUtil.e("updateMessageStatus: invalid messageUri: " + messageUri.toString());
            return false;
        }
        if (statusValues == null) {
            LogUtil.w("updateMessageStatus: empty values to update");
            return false;
        }
        final ContentValues values = new ContentValues();
        if (statusValues.containsKey(MESSAGE_STATUS_READ)) {
            final Integer val = statusValues.getAsInteger(MESSAGE_STATUS_READ);
            if (val != null) {
                // MMS uses the same column name
                values.put(Telephony.Sms.READ, val);
            }
        } else if (statusValues.containsKey(MESSAGE_STATUS_SEEN)) {
            final Integer val = statusValues.getAsInteger(MESSAGE_STATUS_SEEN);
            if (val != null) {
                // MMS uses the same column name
                values.put(Telephony.Sms.SEEN, val);
            }
        }
        if (values.size() < 1) {
            LogUtil.w("updateMessageStatus: no value to update");
            return false;
        }
        // Clear the calling identity and query the database using the phone user id
        // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
        // between the calling uid and the package uid
        final long identity = Binder.clearCallingIdentity();
        try {
            if (getContentResolver().update(
                    messageUri, values, null/*where*/, null/*selectionArgs*/) != 1) {
                LogUtil.e("updateMessageStatus: failed to update database");
                return false;
            }
            return true;
        } catch (SQLiteException e) {
            LogUtil.e("updateMessageStatus: failed to update database", e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return false;
    }

    private static final String ARCHIVE_CONVERSATION_SELECTION = Telephony.Threads._ID + "=?";

    private boolean archiveConversation(long conversationId, boolean archived) {
        final ContentValues values = new ContentValues(1);
        values.put(Telephony.Threads.ARCHIVED, archived ? 1 : 0);
        // Clear the calling identity and query the database using the phone user id
        // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
        // between the calling uid and the package uid
        final long identity = Binder.clearCallingIdentity();
        try {
            if (getContentResolver().update(
                    Telephony.Threads.CONTENT_URI,
                    values,
                    ARCHIVE_CONVERSATION_SELECTION,
                    new String[]{Long.toString(conversationId)}) != 1) {
                LogUtil.e("archiveConversation: failed to update database");
                return false;
            }
            return true;
        } catch (SQLiteException e) {
            LogUtil.e("archiveConversation: failed to update database", e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return false;
    }

    private Uri addSmsDraft(String address, String text, String creator) {
        final ContentValues values = new ContentValues(5);
        values.put(Telephony.Sms.ADDRESS, address);
        values.put(Telephony.Sms.BODY, text);
        values.put(Telephony.Sms.READ, 1);
        values.put(Telephony.Sms.SEEN, 1);
        if (!TextUtils.isEmpty(creator)) {
            values.put(Telephony.Mms.CREATOR, creator);
        }
        // Clear the calling identity and query the database using the phone user id
        // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
        // between the calling uid and the package uid
        final long identity = Binder.clearCallingIdentity();
        try {
            return getContentResolver().insert(Telephony.Sms.Draft.CONTENT_URI, values);
        } catch (SQLiteException e) {
            LogUtil.e("addSmsDraft: failed to store draft message", e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return null;
    }

    private Uri addMmsDraft(Uri contentUri, String creator) {
        byte[] pduData = readPduFromContentUri(contentUri, MAX_MMS_FILE_SIZE);
        if (pduData == null || pduData.length < 1) {
            LogUtil.e("addMmsDraft: empty PDU");
            return null;
        }
        // Clear the calling identity and query the database using the phone user id
        // Otherwise the AppOps check in TelephonyProvider would complain about mismatch
        // between the calling uid and the package uid
        final long identity = Binder.clearCallingIdentity();
        try {
            final GenericPdu pdu = parsePduForAnyCarrier(pduData);
            if (pdu == null) {
                LogUtil.e("addMmsDraft: can't parse input PDU");
                return null;
            }
            if (!(pdu instanceof SendReq)) {
                LogUtil.e("addMmsDraft; invalid MMS type: " + pdu.getClass().getCanonicalName());
                return null;
            }
            final PduPersister persister = PduPersister.getPduPersister(this);
            final Uri uri = persister.persist(
                    pdu,
                    Telephony.Mms.Draft.CONTENT_URI,
                    true/*createThreadId*/,
                    true/*groupMmsEnabled*/,
                    null/*preOpenedFiles*/);
            if (uri == null) {
                LogUtil.e("addMmsDraft: failed to persist message");
                return null;
            }
            final ContentValues values = new ContentValues(3);
            values.put(Telephony.Mms.READ, 1);
            values.put(Telephony.Mms.SEEN, 1);
            if (!TextUtils.isEmpty(creator)) {
                values.put(Telephony.Mms.CREATOR, creator);
            }
            if (SqliteWrapper.update(this, getContentResolver(), uri, values,
                    null/*where*/, null/*selectionArg*/) != 1) {
                LogUtil.e("addMmsDraft: failed to update message");
            }
            return uri;
        } catch (RuntimeException e) {
            LogUtil.e("addMmsDraft: failed to parse input PDU", e);
        } catch (MmsException e) {
            LogUtil.e("addMmsDraft: failed to persist message", e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return null;
    }

    /**
     * Try parsing a PDU without knowing the carrier. This is useful for importing
     * MMS or storing draft when carrier info is not available
     *
     * @param data The PDU data
     * @return Parsed PDU, null if failed to parse
     */
    private static GenericPdu parsePduForAnyCarrier(final byte[] data) {
        GenericPdu pdu = null;
        try {
            pdu = (new PduParser(data, true/*parseContentDisposition*/)).parse();
        } catch (RuntimeException e) {
            LogUtil.w("parsePduForAnyCarrier: Failed to parse PDU with content disposition", e);
        }
        if (pdu == null) {
            try {
                pdu = (new PduParser(data, false/*parseContentDisposition*/)).parse();
            } catch (RuntimeException e) {
                LogUtil.w("parsePduForAnyCarrier: Failed to parse PDU without content disposition",
                        e);
            }
        }
        return pdu;
    }

    @Override
    public boolean getAutoPersistingPref() {
        final SharedPreferences preferences = getSharedPreferences(
                SHARED_PREFERENCES_NAME, MODE_PRIVATE);
        return preferences.getBoolean(PREF_AUTO_PERSISTING, false);
    }

    /**
     * Read pdu from content provider uri.
     *
     * @param contentUri content provider uri from which to read.
     * @param maxSize    maximum number of bytes to read.
     * @return pdu bytes if succeeded else null.
     */
    public byte[] readPduFromContentUri(final Uri contentUri, final int maxSize) {
        // Request one extra byte to make sure file not bigger than maxSize
        byte[] pduData = new byte[maxSize + 1];
        int bytesRead = readPduBytesFromContentUri(contentUri, pduData);
        if (bytesRead <= 0) {
            return null;
        }
        if (bytesRead > maxSize) {
            LogUtil.e("PDU read is too large");
            return null;
        }
        return Arrays.copyOf(pduData, bytesRead);
    }

    /**
     * Read up to length of the pduData array from content provider uri.
     *
     * @param contentUri content provider uri from which to read.
     * @param pduData    the buffer into which the data is read.
     * @return the total number of bytes read into the pduData.
     */
    public int readPduBytesFromContentUri(final Uri contentUri, byte[] pduData) {
        if (contentUri == null) {
            LogUtil.e("Uri is null");
            return 0;
        }
        Callable<Integer> copyPduToArray = new Callable<Integer>() {
            public Integer call() {
                ParcelFileDescriptor.AutoCloseInputStream inStream = null;
                try {
                    ContentResolver cr = MmsService.this.getContentResolver();
                    ParcelFileDescriptor pduFd = cr.openFileDescriptor(contentUri, "r");
                    inStream = new ParcelFileDescriptor.AutoCloseInputStream(pduFd);
                    int bytesRead = inStream.read(pduData, 0, pduData.length);
                    if (bytesRead <= 0) {
                        LogUtil.e("Empty PDU or at end of the file");
                    }
                    return bytesRead;
                } catch (IOException ex) {
                    LogUtil.e("IO exception reading PDU", ex);
                    return 0;
                } finally {
                    if (inStream != null) {
                        try {
                            inStream.close();
                        } catch (IOException ex) {
                        }
                    }
                }
            }
        };

        final Future<Integer> pendingResult = mPduTransferExecutor.submit(copyPduToArray);
        try {
            return pendingResult.get(TASK_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (Exception e) {
            // Typically a timeout occurred - cancel task
            pendingResult.cancel(true);
        }
        return 0;
    }

    /**
     * Write pdu bytes to content provider uri
     *
     * @param contentUri content provider uri to which bytes should be written
     * @param pdu        Bytes to write
     * @return true if all bytes successfully written else false
     */
    public boolean writePduToContentUri(final Uri contentUri, final byte[] pdu) {
        if (contentUri == null || pdu == null) {
            return false;
        }
        final Callable<Boolean> copyDownloadedPduToOutput = new Callable<Boolean>() {
            public Boolean call() {
                ParcelFileDescriptor.AutoCloseOutputStream outStream = null;
                try {
                    ContentResolver cr = MmsService.this.getContentResolver();
                    ParcelFileDescriptor pduFd = cr.openFileDescriptor(contentUri, "w");
                    outStream = new ParcelFileDescriptor.AutoCloseOutputStream(pduFd);
                    outStream.write(pdu);
                    return Boolean.TRUE;
                } catch (IOException ex) {
                    LogUtil.e("IO exception writing PDU", ex);
                    return Boolean.FALSE;
                } finally {
                    if (outStream != null) {
                        try {
                            outStream.close();
                        } catch (IOException ex) {
                        }
                    }
                }
            }
        };

        final Future<Boolean> pendingResult =
                mPduTransferExecutor.submit(copyDownloadedPduToOutput);
        try {
            return pendingResult.get(TASK_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (Exception e) {
            // Typically a timeout occurred - cancel task
            pendingResult.cancel(true);
        }
        return false;
    }
}
