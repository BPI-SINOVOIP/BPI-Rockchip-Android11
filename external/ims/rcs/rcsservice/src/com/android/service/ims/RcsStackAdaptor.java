/*
 * Copyright (c) 2015, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

package com.android.service.ims;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.SystemClock;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.ims.RcsContactUceCapability;

import com.android.ims.ResultCode;
import com.android.ims.RcsPresence;
import com.android.ims.internal.Logger;
import com.android.ims.internal.uce.common.CapInfo;
import com.android.ims.internal.uce.common.StatusCode;
import com.android.ims.internal.uce.common.UceLong;
import com.android.ims.internal.uce.presence.IPresenceService;
import com.android.ims.internal.uce.presence.PresCapInfo;
import com.android.ims.internal.uce.uceservice.IUceService;
import com.android.ims.internal.uce.uceservice.ImsUceManager;
import com.android.service.ims.presence.PresenceBase;
import com.android.service.ims.presence.PresencePublisher;
import com.android.service.ims.presence.SubscribePublisher;

public class RcsStackAdaptor implements PresencePublisher, SubscribePublisher {
    private static final boolean DEBUG = true;

    private static final String PERSIST_SERVICE_NAME =
            "com.android.service.ims.presence.PersistService";
    private static final String PERSIST_SERVICE_PACKAGE = "com.android.service.ims.presence";

    /**
     * PendingIntent action used to retry getting the UCE service. Need an associated
     * BroadcastReceiver.
     */
    public static final String ACTION_RETRY_ALARM = "com.android.service.ims.presence.retry";

    // The logger
    private Logger logger = Logger.getLogger(this.getClass().getName());

    private static final int PRESENCE_INIT_IMS_UCE_SERVICE = 1;

    private Context mContext = null;

    // true when the stack presence service got available. (Called IQPresListener_ServiceAvailable)
    private volatile boolean mImsEnableState = false;

    // provision status can be set by both subscribe and pubilish
    // for unprovisioned for 403 or 404
    private volatile int mPublishingState = PresenceBase.PUBLISH_STATE_NOT_PUBLISHED;

    // It is initializing the stack presence service.
    private volatile boolean mIsIniting = false;

    // The time which the stack presence service got initialized.
    private volatile long mLastInitSubService = -1; //last time which inited the sub service

    // Used for synchronizing
    private final Object mSyncObj = new Object();

    // We need wait RCS stack become unavailable before close RCS service.
    static private boolean sInPowerDown = false;

    // This could happen when the stack first launch or modem panic.
    private static final int PRESENCE_INIT_TYPE_RCS_SERVICE_AVAILABLE =1;

    // The initialization was triggered by retry.
    private static final int PRESENCE_INIT_TYPE_RETRY = 2;

    // The initialization was triggered by retry.
    private static final int PRESENCE_INIT_TYPE_SUB_CHANGED = 3;

    // The maximum retry count for initializing the stack service.
    private static final int MAX_RETRY_COUNT = 6;//Maximum time is 64s

    public boolean isImsEnableState() {
        synchronized (mSyncObj) {
            return mImsEnableState;
        }
    }

    public void setImsEnableState(boolean imsEnableState) {
        synchronized (mSyncObj) {
            logger.debug("imsEnableState=" + imsEnableState);
            mImsEnableState = imsEnableState;
        }
    }

    // The UCE manager for RCS stack.
    private ImsUceManager mImsUceManager = null;

    // The stack RCS Service instance.
    private IUceService mStackService = null;

    // The stack presence service
    private IPresenceService mStackPresService = null;

    // The stack Presence Service handle.
    private int  mStackPresenceServiceHandle;

    // The listener which listen to the response for presence service.
    private StackListener mListenerHandler = null;

    // The handler of the listener.
    private UceLong mListenerHandle = new UceLong();

    // The singleton.
    private static RcsStackAdaptor sInstance = null;

    // the subscription on MSIM devices that is used for presence, since there is no MSIM support.
    private int mAssociatedSubscription = SubscriptionManager.INVALID_SUBSCRIPTION_ID;

    // Only start connecting to the stack after we have received ACTION_UCE_SERVICE_UP.
    private boolean mStackAvailable = false;

    // Constructor
    private RcsStackAdaptor(Context context) {
        mContext = context;

        init();
    }

    public static synchronized RcsStackAdaptor getInstance(Context context) {
        if ((sInstance == null) && (context != null)) {
            sInstance = new RcsStackAdaptor(context);
        }

        return sInstance;
    }

    private Handler mMsgHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);

            logger.debug( "Thread=" + Thread.currentThread().getName() + " received "
                    + msg);
            if(msg == null){
                logger.error("msg=null");
                return;
           }

            switch (msg.what) {
                case PRESENCE_INIT_IMS_UCE_SERVICE:
                    logger.debug("handleMessage  msg=PRESENCE_INIT_IMS_UCE_SERVICE" );
                    registerBroadcastReceiver();
                    doInitImsUceService();
                break;

                default:
                    logger.debug("handleMessage unknown msg=" + msg.what);
            }
        }
    };

    public StackListener getListener(){
        return mListenerHandler;
    }

    public void handleAssociatedSubscriptionChanged(int newSubId) {
        synchronized (mSyncObj) {
            if (mAssociatedSubscription == newSubId) {
                return;
            }
            mAssociatedSubscription = newSubId;

            if (!SubscriptionManager.isValidSubscriptionId(mAssociatedSubscription)) {
                destroyStackConnection();
                return;
            }

            SubscriptionManager subscriptionManager = mContext.getSystemService(
                    SubscriptionManager.class);
            if (subscriptionManager == null) {
                logger.error("handleAssociatedSubscriptionChanged: error getting sub manager");
                return;
            }
            if (mStackAvailable) {
                startInitPresenceTimer(0, PRESENCE_INIT_TYPE_SUB_CHANGED);
            }
        }
    }

    @Override
    public int getStackStatusForCapabilityRequest() {
        if (!RcsSettingUtils.getCapabilityDiscoveryEnabled(mAssociatedSubscription)) {
            logger.error("getCapabilityDiscoveryEnabled = false");
            return ResultCode.ERROR_SERVICE_NOT_ENABLED;
        }

        int ret = checkStackStatus();
        if (ret != ResultCode.SUCCESS) {
            logger.error("checkStackAndPublish ret=" + ret);
            return ret;
        }

        if (!isPublished()) {
            logger.error("checkStackAndPublish ERROR_SERVICE_NOT_PUBLISHED");
            return ResultCode.ERROR_SERVICE_NOT_PUBLISHED;
        }

        return ResultCode.SUCCESS;
    }

    private boolean isPublished() {
        if (getPublisherState() != PresenceBase.PUBLISH_STATE_200_OK) {
            logger.error("Didnt' publish properly");
            return false;
        }

        return true;
    }

    @Override
    public void updatePublisherState(@PresenceBase.PresencePublishState int publishState) {
        synchronized (mSyncObj) {
            logger.print("mPublishingState=" + mPublishingState + " publishState=" + publishState);
            mPublishingState = publishState;
        }
        Intent publishIntent = new Intent(RcsPresence.ACTION_PUBLISH_STATE_CHANGED);
        publishIntent.putExtra(RcsPresence.EXTRA_PUBLISH_STATE, publishState);
        // Start PersistService and broadcast to other receivers that are listening
        // dynamically.
        mContext.sendStickyBroadcast(publishIntent);
        launchPersistService(publishIntent);
    }

    @Override
    public @PresenceBase.PresencePublishState int getPublisherState() {
        synchronized (mSyncObj) {
            return mPublishingState;
        }
    }

    private int checkStackStatus() {
        synchronized (mSyncObj) {
            if (!RcsSettingUtils.isEabProvisioned(mContext, mAssociatedSubscription)) {
                logger.error("Didn't get EAB provisioned by DM");
                return ResultCode.ERROR_SERVICE_NOT_ENABLED;
            }

            // Don't send request to RCS stack when it is under powering off.
            // RCS stack is sending UNPUBLISH. It could get race PUBLISH trigger under the case.
            if (sInPowerDown) {
                logger.error("checkStackStatus: under powering off");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }

            if (mStackService == null) {
                logger.error("checkStackStatus: mStackService == null");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }

            if (mStackPresService == null) {
                logger.error("Didn't init sub rcs service.");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }

            if (!mImsEnableState) {
                logger.error("mImsEnableState = false");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }
        }

        return ResultCode.SUCCESS;
    }

    @Override
    public int requestCapability(String[] formattedContacts, int taskId) {
        logger.print("requestCapability formattedContacts=" + formattedContacts);

        int ret = ResultCode.SUCCESS;
        try {
            synchronized (mSyncObj) {
                StatusCode retCode;
                if (formattedContacts.length == 1) {
                    retCode = mStackPresService.getContactCap(
                            mStackPresenceServiceHandle, formattedContacts[0], taskId);
                } else {
                    retCode = mStackPresService.getContactListCap(
                            mStackPresenceServiceHandle, formattedContacts, taskId);
                }

                logger.print("GetContactListCap retCode=" + retCode);

                ret = RcsUtils.statusCodeToResultCode(retCode.getStatusCode());
            }

            logger.debug("requestCapability ret=" + ret);
        }catch(Exception e){
            logger.error("requestCapability exception", e);
            ret = ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
        }

        return  ret;
    }

    @Override
    public int requestAvailability(String formattedContact, int taskId) {
        logger.debug("requestAvailability ...");

        int ret = ResultCode.SUCCESS;
        try{
            synchronized (mSyncObj) {
                StatusCode retCode = mStackPresService.getContactCap(
                        mStackPresenceServiceHandle, formattedContact, taskId);
                logger.print("getContactCap retCode=" + retCode);

                ret = RcsUtils.statusCodeToResultCode(retCode.getStatusCode());
            }
            logger.debug("requestAvailability ret=" + ret);
        }catch(Exception e){
            logger.error("requestAvailability exception", e);
            ret = ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
        }

        return  ret;
    }

    @Override
    public int requestPublication(RcsContactUceCapability capabilities, String myUri, int taskId) {
        logger.debug("requestPublication ...");

         // Don't use checkStackAndPublish()
         // since it will check publish status which in dead loop.
         int ret = checkStackStatus();
         if(ret != ResultCode.SUCCESS){
             logger.error("requestPublication ret=" + ret);
             return ret;
         }
        if (myUri == null) {
            logger.error("Didn't find number or impu.");
            return ResultCode.PUBLISH_GENERIC_FAILURE;
        }
        try {
            PresCapInfo pMyCapInfo = new PresCapInfo();
            // Fill cap info
            pMyCapInfo.setContactUri(myUri);

            CapInfo capInfo = new CapInfo();
            capInfo.setIpVoiceSupported(capabilities.isCapable(
                    RcsContactUceCapability.CAPABILITY_IP_VOICE_CALL));
            capInfo.setIpVideoSupported(capabilities.isCapable(
                    RcsContactUceCapability.CAPABILITY_IP_VIDEO_CALL));
            capInfo.setCdViaPresenceSupported(true);

            capInfo.setFtSupported(false); // TODO: support FT
            capInfo.setImSupported(false);//TODO: support chat
            capInfo.setFullSnFGroupChatSupported(false); //TODO: support chat

            pMyCapInfo.setCapInfo(capInfo);

            logger.print( "myNumUri = " + myUri
                    + " audioSupported =  " + capInfo.isIpVoiceSupported()
                    + " videoSupported=  " + capInfo.isIpVideoSupported()
                    );


            synchronized (mSyncObj) {
                StatusCode status = mStackPresService.publishMyCap(
                        mStackPresenceServiceHandle, pMyCapInfo, taskId);
                logger.print("PublishMyCap status=" + status.getStatusCode());
                ret = RcsUtils.statusCodeToResultCode(status.getStatusCode());
            }

            logger.debug("requestPublication ret=" + ret);
            if (ret != ResultCode.SUCCESS) {
                logger.error("requestPublication remove taskId=" + taskId);
                return ret;
            }
        } catch (RemoteException e) {
            e.printStackTrace();
            logger.error("Exception when call mStackPresService.getContactCap");
            logger.error("requestPublication remove taskId=" + taskId);

            return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
        }

        return  ResultCode.SUCCESS;
    }

    private void launchPersistService(Intent intent) {
        ComponentName component = new ComponentName(PERSIST_SERVICE_PACKAGE,
                PERSIST_SERVICE_NAME);
        intent.setComponent(component);
        mContext.startService(intent);
    }

    private void createListeningThread() {
        HandlerThread listenerThread = new HandlerThread("Listener",
                android.os.Process.THREAD_PRIORITY_BACKGROUND);

        listenerThread.start();
        Looper listenerLooper = listenerThread.getLooper();
        mListenerHandler = new StackListener(mContext, listenerLooper);
    }

    private void initImsUceService(){
        // Send message to avoid ANR
        Message reinitMessage = mMsgHandler.obtainMessage(
                PRESENCE_INIT_IMS_UCE_SERVICE, null);
        mMsgHandler.sendMessage(reinitMessage);
    }

    private void registerBroadcastReceiver() {
        synchronized (mSyncObj) {
            logger.debug("registerBroadcastReceiver");

            IntentFilter filter = new IntentFilter();
            filter.addAction(ImsUceManager.ACTION_UCE_SERVICE_UP);
            filter.addAction(ImsUceManager.ACTION_UCE_SERVICE_DOWN);

            mRcsServiceReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    // do something based on the intent's action
                    logger.print("onReceive intent " + intent);
                    String action = intent.getAction();
                    if (ImsUceManager.ACTION_UCE_SERVICE_UP.equals(action)) {
                        mStackAvailable = true;
                        startInitPresenceTimer(0, PRESENCE_INIT_TYPE_RCS_SERVICE_AVAILABLE);
                    } else if (ImsUceManager.ACTION_UCE_SERVICE_DOWN.equals(action)) {
                        mStackAvailable = false;
                        clearImsUceService();
                    } else {
                        logger.debug("unknown intent " + intent);
                    }
                }
            };

            mContext.registerReceiver(mRcsServiceReceiver, filter);
        }
    }

    private void doInitImsUceService(){
        synchronized (mSyncObj) {
            if (mStackService != null) {
                logger.debug("registerBroadcastReceiver mStackService != null");
                return;
            }

            if (mImsUceManager == null) {
                mImsUceManager = ImsUceManager.getInstance(mContext);
                if (mImsUceManager == null) {
                    logger.error("Can't init mImsUceManager");
                    return;
                }
            }

            mImsUceManager.createUceService(false);
            mStackService = mImsUceManager.getUceServiceInstance();
            logger.debug("doInitImsUceService mStackService=" + mStackService);

            // Do not connect to vendor UCE stack until ACTION_UCE_SERVICE_UP is called.
            // The intent is sticky, so if we crash, we will get the UCE_SERVICE_UP intent again.
        }
    }

    private PendingIntent mRetryAlarmIntent = null;
    private AlarmManager mAlarmManager = null;
    private BroadcastReceiver mRcsServiceReceiver = null;

    /*
     * Init All Sub service of RCS
     */
    int initAllSubRcsServices(IUceService uceService, int currentRetry) {
        int ret = -1;
        synchronized (mSyncObj) {
            logger.print("Create UCE service connection for sub " + mAssociatedSubscription);
            if (uceService == null) {
                logger.error("initAllSubRcsServices : uceService is NULL");
                mIsIniting = false;
                mLastInitSubService = -1;
                return ret;
            }

            try {
                destroyStackConnection();

                if (!SubscriptionManager.isValidSubscriptionId(mAssociatedSubscription)) {
                    logger.print("initAllService : invalid sub id, stopping creation...");
                    mIsIniting = false;
                    mLastInitSubService = -1;
                    // Do not create a new presence service for invalid sub id.
                    return 0;
                }

                boolean serviceAvailable;
                serviceAvailable = uceService.getServiceStatus();
                //init only once and ensure connection to UCE  service is available.
                if (serviceAvailable && mStackPresService == null && mStackService != null) {
                    logger.print("initAllSubRcsServices : create ");
                    int handle = createStackConnection();
                    logger.print("initAllSubRcsServices: handle=" + mStackPresenceServiceHandle +
                            ", service=" + mStackPresService);
                    // If the service handle is -1, then creating the service failed somehow.
                    // schedule a retry.
                    if (handle < 0) {
                        logger.error("initAllService : service handle < 0, retrying...");
                        mIsIniting = false;
                        mLastInitSubService = -1;
                        return ret;
                    }
                    ret = 0;
                 } else {
                    logger.error("initAllService : serviceStatus =  false - serviceStatus: "
                            + serviceAvailable + ", mStackPresService: " + mStackPresService
                            + ", mStackService: " + mStackService);
                }
            } catch (RemoteException e) {
                logger.error("initAllServices :  DeadObjectException dialog  ");
                e.printStackTrace();
            }
            mIsIniting = false;
        }
        return ret;
    }

    private void destroyStackConnection() {
        synchronized (mSyncObj) {
            try {
                if (mStackPresService != null) {
                    logger.print("RemoveListener and presence service");
                    mStackPresService.removeListener(mStackPresenceServiceHandle,
                            mListenerHandle);
                }
                if (mStackService != null) {
                    mStackService.destroyPresenceService(mStackPresenceServiceHandle);
                }
                mStackPresService = null;
            } catch (RemoteException e) {
                logger.error("destroyStackConnection :  exception " + e.getMessage());
                e.printStackTrace();
            }
        }
    }

    private int createStackConnection() throws RemoteException {
        TelephonyManager tm = mContext.getSystemService(TelephonyManager.class);
        SubscriptionManager sm = mContext.getSystemService(SubscriptionManager.class);
        if (tm == null) return -1;
        if (sm == null) return -1;
        SubscriptionInfo info = sm.getActiveSubscriptionInfo(mAssociatedSubscription);
        if (info == null) {
            logger.error("handleAssociatedSubscriptionChanged: sub id does not have valid info");
            return -1;
        }
        String associatedIccId = info.getIccId();
        boolean isMsim = tm.getSupportedModemCount() > 1;
        synchronized (mSyncObj) {
            if (isMsim) {
                mStackPresenceServiceHandle = mStackService.createPresenceServiceForSubscription(
                        mListenerHandler.mPresenceListener, mListenerHandle, associatedIccId);
            } else {
                // createPresenceServiceForSubscription doesnt seem to work on older single sim
                // devices. Use deprecated API for these devices.
                mStackPresenceServiceHandle = mStackService.createPresenceService(
                        mListenerHandler.mPresenceListener, mListenerHandle);
            }
            // If the service handle is -1, then creating the service failed somehow.
            if (mStackPresenceServiceHandle < 0) {
                return mStackPresenceServiceHandle;
            }
            if (isMsim) {
                mStackPresService = mStackService.getPresenceServiceForSubscription(
                        associatedIccId);
            } else {
                // getPresenceServiceForSubscription doesnt seem to work on older single SIM
                // devices. Use deprecated API for these devices.
                mStackPresService = mStackService.getPresenceService();
            }
            return mStackPresenceServiceHandle;
        }
    }

    public void startInitThread(int times){
        final int currentRetry = times;
        Thread thread = new Thread(() -> {
            synchronized (mSyncObj) {
                if (currentRetry >= 0 && currentRetry <= MAX_RETRY_COUNT) {
                    refreshUceService();

                    if (initAllSubRcsServices(mStackService, currentRetry) >= 0) {
                        logger.debug("init sub rcs service successfully.");
                        if (mRetryAlarmIntent != null) {
                            mAlarmManager.cancel(mRetryAlarmIntent);
                        }
                    } else {
                        startInitPresenceTimer(currentRetry + 1, PRESENCE_INIT_TYPE_RETRY);
                    }
                } else {
                    logger.debug("Retry times=" + currentRetry);
                    if (mRetryAlarmIntent != null) {
                        mAlarmManager.cancel(mRetryAlarmIntent);
                    }
                }
            }
        }, "initAllSubRcsServices thread");

        thread.start();
    }

    private void init() {
        createListeningThread();
        logger.debug("after createListeningThread");

        if(mAlarmManager == null){
            mAlarmManager = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        }

        initImsUceService();

        setInPowerDown(false);
        logger.debug("init finished");
    }

    private void startInitPresenceTimer(int times, int initType){
        synchronized (mSyncObj) {
            logger.print("set the retry alarm, times=" + times + " initType=" + initType +
                    " mIsIniting=" + mIsIniting);
            if(mIsIniting){
                //initing is on going in 5 seconds, discard this one.
                if(mLastInitSubService != -1 &&
                        System.currentTimeMillis() - mLastInitSubService < 5000){
                    logger.print("already in initing. ignore it");
                    return;
                }//else suppose the init has problem. so continue
            }

            mIsIniting = true;

            Intent intent = new Intent(ACTION_RETRY_ALARM);
            intent.putExtra("times", times);
            intent.setPackage(mContext.getPackageName());
            mRetryAlarmIntent = PendingIntent.getBroadcast(mContext, 0, intent,
                    PendingIntent.FLAG_UPDATE_CURRENT);

            // Wait for 1s to ignore duplicate init request as possible as we can.
            long timeSkip = 1000;
            if(times < 0 || times >= MAX_RETRY_COUNT){
                times = MAX_RETRY_COUNT;
            }

            //Could failed to cancel a timer in 1s. So use exponential retry to make sure it
            //will be stopped for non-VoLte SIM.
            timeSkip = (timeSkip << times);
            logger.debug("timeSkip = " + timeSkip);

            mLastInitSubService = System.currentTimeMillis();

            //the timer intent could have a longer delay. call directly at first time
            if(times == 0) {
                startInitThread(0);
            } else {
                mAlarmManager.setExact(AlarmManager.ELAPSED_REALTIME_WAKEUP,
                        SystemClock.elapsedRealtime() + timeSkip, mRetryAlarmIntent);
            }
        }
    }

    private void refreshUceService() {
        synchronized (mSyncObj) {
            logger.debug("refreshUceService mImsUceManager=" + mImsUceManager +
                    " mStackService=" + mStackService);

            if (mImsUceManager == null) {
                mImsUceManager = ImsUceManager.getInstance(mContext);
                if (mImsUceManager == null) {
                    logger.error("Can't init mImsUceManager");
                    return;
                }
            }

            if (mStackService == null) {
                mImsUceManager.createUceService(false);
                mStackService = mImsUceManager.getUceServiceInstance();
            }

            logger.debug("refreshUceService mStackService=" + mStackService);
        }
    }

    private void clearImsUceService() {
        destroyStackConnection();
        mImsUceManager = null;
        mStackService = null;
        mStackPresService = null;
    }

    public void finish() {
        if(mRetryAlarmIntent != null){
            mAlarmManager.cancel(mRetryAlarmIntent);
            mRetryAlarmIntent = null;
        }

        if (mRcsServiceReceiver != null) {
            mContext.unregisterReceiver(mRcsServiceReceiver);
            mRcsServiceReceiver = null;
        }

        clearImsUceService();
    }

    protected void finalize() throws Throwable {
        super.finalize();
        finish();
    }

    static public boolean isInPowerDown() {
        return sInPowerDown;
    }

    static void setInPowerDown(boolean inPowerDown) {
        sInPowerDown = inPowerDown;
    }
}

