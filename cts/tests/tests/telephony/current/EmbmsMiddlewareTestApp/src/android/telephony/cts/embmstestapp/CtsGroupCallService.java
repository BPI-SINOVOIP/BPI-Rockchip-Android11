/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.telephony.cts.embmstestapp;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.telephony.mbms.GroupCall;
import android.telephony.mbms.MbmsErrors;
import android.telephony.mbms.MbmsGroupCallSessionCallback;
import android.telephony.mbms.GroupCallCallback;
import android.telephony.mbms.vendor.MbmsGroupCallServiceBase;
import android.util.Log;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

public class CtsGroupCallService extends MbmsGroupCallServiceBase {
    private static final Set<String> ALLOWED_PACKAGES = new HashSet<String>() {{
        add("android.telephony.cts");
    }};
    private static final String TAG = "EmbmsTestGroupCall";

    public static final String METHOD_INITIALIZE = "initialize";
    public static final String METHOD_START_GROUP_CALL = "startGroupCall";
    public static final String METHOD_UPDATE_GROUP_CALL = "updateGroupCall";
    public static final String METHOD_STOP_GROUP_CALL = "stopGroupCall";
    public static final String METHOD_CLOSE = "close";

    public static final String CONTROL_INTERFACE_ACTION =
            "android.telephony.cts.embmstestapp.ACTION_CONTROL_MIDDLEWARE";
    public static final ComponentName CONTROL_INTERFACE_COMPONENT =
            ComponentName.unflattenFromString(
                    "android.telephony.cts.embmstestapp/.CtsGroupCallService");

    private MbmsGroupCallSessionCallback mAppCallback;
    private GroupCallCallback mGroupCallCallback;

    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private List<List> mReceivedCalls = new LinkedList<>();
    private int mErrorCodeOverride = MbmsErrors.SUCCESS;

    @Override
    public int initialize(MbmsGroupCallSessionCallback callback, int subId) {
        mReceivedCalls.add(Arrays.asList(METHOD_INITIALIZE, subId));
        if (mErrorCodeOverride != MbmsErrors.SUCCESS) {
            return mErrorCodeOverride;
        }

        int packageUid = Binder.getCallingUid();
        String[] packageNames = getPackageManager().getPackagesForUid(packageUid);
        if (packageNames == null) {
            return MbmsErrors.InitializationErrors.ERROR_APP_PERMISSIONS_NOT_GRANTED;
        }
        boolean isUidAllowed = Arrays.stream(packageNames).anyMatch(ALLOWED_PACKAGES::contains);
        if (!isUidAllowed) {
            return MbmsErrors.InitializationErrors.ERROR_APP_PERMISSIONS_NOT_GRANTED;
        }

        mHandler.post(() -> {
            if (mAppCallback == null) {
                mAppCallback = callback;
            } else {
                callback.onError(
                        MbmsErrors.InitializationErrors.ERROR_DUPLICATE_INITIALIZE, "");
                return;
            }
            callback.onMiddlewareReady();
        });
        return MbmsErrors.SUCCESS;
    }

    @Override
    public int startGroupCall(final int subscriptionId, final long tmgi,
            final List<Integer> saiArray, final List<Integer> frequencyArray,
            final GroupCallCallback callback) {
        mReceivedCalls.add(Arrays.asList(METHOD_START_GROUP_CALL, subscriptionId, tmgi, saiArray,
                frequencyArray));
        if (mErrorCodeOverride != MbmsErrors.SUCCESS) {
            return mErrorCodeOverride;
        }

        mGroupCallCallback = callback;
        mHandler.post(() -> callback.onGroupCallStateChanged(GroupCall.STATE_STARTED,
                GroupCall.REASON_BY_USER_REQUEST));
        return MbmsErrors.SUCCESS;
    }

    @Override
    public void updateGroupCall(int subscriptionId, long tmgi,
            List<Integer> saiArray, List<Integer> frequencyArray) {
        mReceivedCalls.add(Arrays.asList(METHOD_UPDATE_GROUP_CALL,
                subscriptionId, tmgi, saiArray, frequencyArray));
    }

    @Override
    public void stopGroupCall(int subscriptionId, long tmgi) {
        mReceivedCalls.add(Arrays.asList(METHOD_STOP_GROUP_CALL, subscriptionId, tmgi));
    }

    @Override
    public void dispose(int subscriptionId) {
        mReceivedCalls.add(Arrays.asList(METHOD_CLOSE, subscriptionId));
    }

    @Override
    public void onAppCallbackDied(int uid, int subscriptionId) {
        mAppCallback = null;
    }

    private final IBinder mControlInterface = new ICtsGroupCallMiddlewareControl.Stub() {
        @Override
        public void reset() {
            mReceivedCalls.clear();
            mHandler.removeCallbacksAndMessages(null);
            mAppCallback = null;
            mErrorCodeOverride = MbmsErrors.SUCCESS;
        }

        @Override
        public List getGroupCallSessionCalls() {
            return mReceivedCalls;
        }

        @Override
        public void forceErrorCode(int error) {
            mErrorCodeOverride = error;
        }

        @Override
        public void fireErrorOnGroupCall(int errorCode, String message) {
            mHandler.post(() -> mGroupCallCallback.onError(errorCode, message));
        }

        @Override
        public void fireErrorOnSession(int errorCode, String message) {
            mHandler.post(() -> mAppCallback.onError(errorCode, message));
        }

        @Override
        public void fireGroupCallStateChanged(int state, int reason) {
            mHandler.post(() -> mGroupCallCallback.onGroupCallStateChanged(state, reason));
        }

        @Override
        public void fireBroadcastSignalStrengthUpdated(int signalStrength) {
            mHandler.post(
                    () -> mGroupCallCallback.onBroadcastSignalStrengthUpdated(signalStrength));
        }

        @Override
        public void fireAvailableSaisUpdated(List currentSais, List availableSais) {
            mHandler.post(() -> mAppCallback.onAvailableSaisUpdated(currentSais, availableSais));
        }

        @Override
        public void fireServiceInterfaceAvailable(String interfaceName, int index) {
            mHandler.post(() -> mAppCallback.onServiceInterfaceAvailable(interfaceName, index));
        }
    };

    @Override
    public void onDestroy() {
        mHandlerThread.quitSafely();
        logd("CtsGroupCallService onDestroy");
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        logd("CtsGroupCallService onBind");
        if (CONTROL_INTERFACE_ACTION.equals(intent.getAction())) {
            logd("CtsGroupCallService control interface bind");
            return mControlInterface;
        }
        IBinder binder = super.onBind(intent);

        if (mHandlerThread != null && mHandlerThread.isAlive()) {
            return binder;
        }

        mHandlerThread = new HandlerThread("CtsGroupCallServiceWorker");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        return binder;
    }

    private static void logd(String s) {
        Log.d(TAG, s);
    }

    private void checkInitialized() {
        if (mAppCallback == null) {
            throw new IllegalStateException("Not yet initialized");
        }
    }
}
