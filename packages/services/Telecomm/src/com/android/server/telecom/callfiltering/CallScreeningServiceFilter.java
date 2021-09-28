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

package com.android.server.telecom.callfiltering;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.provider.CallLog;
import android.telecom.Log;
import android.telecom.TelecomManager;

import com.android.internal.telecom.ICallScreeningAdapter;
import com.android.internal.telecom.ICallScreeningService;
import com.android.server.telecom.AppLabelProxy;
import com.android.server.telecom.Call;
import com.android.server.telecom.CallScreeningServiceHelper;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.LogUtils;
import com.android.server.telecom.ParcelableCallUtils;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;

public class CallScreeningServiceFilter extends CallFilter {
    public static final int PACKAGE_TYPE_CARRIER = 0;
    public static final int PACKAGE_TYPE_DEFAULT_DIALER = 1;
    public static final int PACKAGE_TYPE_USER_CHOSEN = 2;
    public static final long CALL_SCREENING_FILTER_TIMEOUT = 5000;

    private final Call mCall;
    private final String mPackageName;
    private final int mPackagetype;
    private PackageManager mPackageManager;
    private Context mContext;
    private CallScreeningServiceConnection mConnection;
    private final CallsManager mCallsManager;
    private CharSequence mAppName;
    private final ParcelableCallUtils.Converter mParcelableCallUtilsConverter;

    private class CallScreeningAdapter extends ICallScreeningAdapter.Stub {
        private CompletableFuture<CallFilteringResult> mResultFuture;

        public CallScreeningAdapter(CompletableFuture<CallFilteringResult> resultFuture) {
            mResultFuture = resultFuture;
        }

        @Override
        public void allowCall(String callId) {
            Long token = Binder.clearCallingIdentity();
            Log.startSession("NCSSF.aC");
            try {
                if (mCall == null || (!mCall.getId().equals(callId))) {
                    Log.w(this, "allowCall, unknown call id: %s", callId);
                }
                Log.addEvent(mCall, LogUtils.Events.SCREENING_COMPLETED, mPriorStageResult);
                mResultFuture.complete(mPriorStageResult);
            } finally {
                unbindCallScreeningService();
                Binder.restoreCallingIdentity(token);
                Log.endSession();
            }
        }

        @Override
        public void disallowCall(String callId, boolean shouldReject,
                boolean shouldAddToCallLog, boolean shouldShowNotification,
                ComponentName componentName) {
            long token = Binder.clearCallingIdentity();
            Log.startSession("NCSSF.dC");
            try {
                if (mCall != null && mCall.getId().equals(callId)) {
                    CallFilteringResult result = new CallFilteringResult.Builder()
                            .setShouldAllowCall(false)
                            .setShouldReject(shouldReject)
                            .setShouldSilence(false)
                            .setShouldAddToCallLog(shouldAddToCallLog
                                    || packageTypeShouldAdd(mPackagetype))
                            .setShouldShowNotification(shouldShowNotification)
                            .setCallBlockReason(CallLog.Calls.BLOCK_REASON_CALL_SCREENING_SERVICE)
                            .setCallScreeningAppName(mAppName)
                            .setCallScreeningComponentName(componentName.flattenToString())
                            .setContactExists(mPriorStageResult.contactExists)
                            .build();
                    Log.addEvent(mCall, LogUtils.Events.SCREENING_COMPLETED, result);
                    mResultFuture.complete(result);
                } else {
                    Log.w(this, "disallowCall, unknown call id: %s", callId);
                    mResultFuture.complete(mPriorStageResult);
                }
            } finally {
                unbindCallScreeningService();
                Log.endSession();
                Binder.restoreCallingIdentity(token);
            }
        }

        @Override
        public void silenceCall(String callId) {
            long token = Binder.clearCallingIdentity();
            Log.startSession("NCSSF.sC");
            try {
                if (mCall != null && mCall.getId().equals(callId)) {
                    CallFilteringResult result = new CallFilteringResult.Builder()
                            .setShouldAllowCall(true)
                            .setShouldReject(false)
                            .setShouldSilence(true)
                            .setShouldAddToCallLog(true)
                            .setShouldShowNotification(true)
                            .setContactExists(mPriorStageResult.contactExists)
                            .build();
                    Log.addEvent(mCall, LogUtils.Events.SCREENING_COMPLETED, result);
                    mResultFuture.complete(result);
                } else {
                    Log.w(this, "silenceCall, unknown call id: %s", callId);
                    mResultFuture.complete(mPriorStageResult);
                }
            } finally {
                unbindCallScreeningService();
                Log.endSession();
                Binder.restoreCallingIdentity(token);
            }
        }

        @Override
        public void screenCallFurther(String callId) {
            if (mPackagetype != PACKAGE_TYPE_DEFAULT_DIALER) {
                throw new SecurityException("Only the default/system dialer may request screen via"
                    + "background call audio");
            }
            // TODO: add permission check for the additional role-based permission
            long token = Binder.clearCallingIdentity();
            Log.startSession("NCSSF.sCF");

            try {
                if (mCall != null && mCall.getId().equals(callId)) {
                    CallFilteringResult result = new CallFilteringResult.Builder()
                            .setShouldAllowCall(true)
                            .setShouldReject(false)
                            .setShouldSilence(false)
                            .setShouldScreenViaAudio(true)
                            .setCallScreeningAppName(mAppName)
                            .setContactExists(mPriorStageResult.contactExists)
                            .build();
                    Log.addEvent(mCall, LogUtils.Events.SCREENING_COMPLETED, result);
                    mResultFuture.complete(result);
                } else {
                    Log.w(this, "screenCallFurther, unknown call id: %s", callId);
                    mResultFuture.complete(mPriorStageResult);
                }
            } finally {
                unbindCallScreeningService();
                Log.endSession();
                Binder.restoreCallingIdentity(token);
            }
        }
    }

    private class CallScreeningServiceConnection implements ServiceConnection {
        private CompletableFuture<CallFilteringResult> mResultFuture;

        public CallScreeningServiceConnection(CompletableFuture<CallFilteringResult> resultFuture) {
            mResultFuture = resultFuture;
        }

        @Override
        public void onServiceConnected(ComponentName componentName, IBinder service) {
            ICallScreeningService callScreeningService =
                    ICallScreeningService.Stub.asInterface(service);
            try {
                callScreeningService.screenCall(new CallScreeningAdapter(mResultFuture),
                        mParcelableCallUtilsConverter.
                                toParcelableCallForScreening(mCall, isSystemDialer()));
            } catch (RemoteException e) {
                Log.e(this, e, "Failed to set the call screening adapter");
                mResultFuture.complete(mPriorStageResult);
            }
            Log.addEvent(mCall, LogUtils.Events.SCREENING_BOUND, componentName);
            Log.i(this, "Binding completed.");
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            mResultFuture.complete(mPriorStageResult);
            Log.i(this, "Service disconnected.");
        }

        @Override
        public void onBindingDied(ComponentName name) {
            mResultFuture.complete(mPriorStageResult);
            Log.i(this, "Binding died.");
        }

        @Override
        public void onNullBinding(ComponentName name) {
            mResultFuture.complete(mPriorStageResult);
            Log.i(this, "Null binding.");
            unbindCallScreeningService();
        }
    }

    public CallScreeningServiceFilter(
            Call call,
            String packageName,
            int packageType,
            Context context,
            CallsManager callsManager,
            AppLabelProxy appLabelProxy,
            ParcelableCallUtils.Converter parcelableCallUtilsConverter) {
        super();
        mCall = call;
        mPackageName = packageName;
        mPackagetype = packageType;
        mContext = context;
        mPackageManager = mContext.getPackageManager();
        mCallsManager = callsManager;
        mAppName = appLabelProxy.getAppLabel(mPackageName);
        mParcelableCallUtilsConverter = parcelableCallUtilsConverter;
    }

    @Override
    public CompletionStage<CallFilteringResult> startFilterLookup(
            CallFilteringResult priorStageResult) {
        mPriorStageResult = priorStageResult;
        if (mPackageName == null) {
            return CompletableFuture.completedFuture(priorStageResult);
        }

        if (!priorStageResult.shouldAllowCall) {
            // Call already blocked by other filters, no need to bind to call screening service.
            return CompletableFuture.completedFuture(priorStageResult);
        }

        if (priorStageResult.contactExists && (!hasReadContactsPermission())) {
            // Binding to the call screening service will be skipped if it does NOT hold
            // READ_CONTACTS permission and the number is in the user’s contacts
            return CompletableFuture.completedFuture(priorStageResult);
        }

        CompletableFuture<CallFilteringResult> resultFuture = new CompletableFuture<>();

        bindCallScreeningService(resultFuture);
        return resultFuture;
    }

    @Override
    public String toString() {
        return super.toString() + ": " + mPackageName;
    }

    private boolean hasReadContactsPermission() {
        int permission = PackageManager.PERMISSION_DENIED;
        if (mPackagetype == PACKAGE_TYPE_CARRIER || mPackagetype == PACKAGE_TYPE_DEFAULT_DIALER) {
            permission = PackageManager.PERMISSION_GRANTED;
        } else if (mPackageManager != null) {
            permission = mPackageManager.checkPermission(Manifest.permission.READ_CONTACTS,
                    mPackageName);
        }
        return permission == PackageManager.PERMISSION_GRANTED;
    }

    private void bindCallScreeningService(
            CompletableFuture<CallFilteringResult> resultFuture) {
        mConnection = new CallScreeningServiceConnection(resultFuture);
        if (!CallScreeningServiceHelper.bindCallScreeningService(mContext,
                mCallsManager.getCurrentUserHandle(), mPackageName, mConnection)) {
            Log.i(this, "Call screening service binding failed.");
            resultFuture.complete(mPriorStageResult);
        }
    }

    public void unbindCallScreeningService() {
        if (mConnection != null) {
            try {
                mContext.unbindService(mConnection);
            } catch (IllegalArgumentException e) {
                Log.i(this, "Exception when unbind service %s : %s", mConnection,
                        e.getMessage());
            }
        }
        mConnection = null;
    }

    private boolean isSystemDialer() {
        if (mPackagetype != PACKAGE_TYPE_DEFAULT_DIALER) {
            return false;
        } else {
            return mPackageName.equals(
                    mContext.getSystemService(TelecomManager.class).getSystemDialerPackage());
        }
    }

    private boolean packageTypeShouldAdd(int packageType) {
        return packageType != PACKAGE_TYPE_CARRIER;
    }
}
