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

package com.android.server.telecom.tests;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.os.IBinder;
import android.os.UserHandle;
import android.provider.CallLog;
import android.telecom.CallScreeningService;
import android.telecom.ParcelableCall;
import android.telecom.TelecomManager;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.internal.telecom.ICallScreeningAdapter;
import com.android.internal.telecom.ICallScreeningService;
import com.android.server.telecom.AppLabelProxy;
import com.android.server.telecom.Call;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.ParcelableCallUtils;
import com.android.server.telecom.PhoneAccountRegistrar;
import com.android.server.telecom.callfiltering.CallFilteringResult;
import com.android.server.telecom.callfiltering.CallScreeningServiceFilter;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

import java.util.Collections;
import java.util.concurrent.CompletionStage;
import java.util.concurrent.TimeUnit;

@RunWith(JUnit4.class)
public class CallScreeningServiceFilterTest extends TelecomTestCase {
    static @Mock Call mCall;
    @Mock Context mContext;
    @Mock TelecomManager mTelecomManager;
    @Mock PackageManager mPackageManager;
    @Mock CallsManager mCallsManager;
    @Mock AppLabelProxy mAppLabelProxy;
    @Mock ParcelableCallUtils.Converter mParcelableCallUtilsConverter;
    @Mock PhoneAccountRegistrar mPhoneAccountRegistrar;
    @Mock ICallScreeningService mCallScreeningService;
    @Mock IBinder mBinder;

    private static final String CALL_ID = "u89prgt9ps78y5";
    private static final String PKG_NAME = "com.android.services.telecom.tests";
    private static final String APP_NAME = "TeleTestApp";
    private static final String CLS_NAME = "CallScreeningService";
    private static final ComponentName COMPONENT_NAME = new ComponentName(PKG_NAME, CLS_NAME);
    private ResolveInfo mResolveInfo;
    private CallFilteringResult inputResult;

    private static final CallFilteringResult PASS_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(true)
            .setShouldReject(false)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(true)
            .build();

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();

        mResolveInfo =  new ResolveInfo() {{
            serviceInfo = new ServiceInfo();
            serviceInfo.packageName = PKG_NAME;
            serviceInfo.name = CLS_NAME;
            serviceInfo.permission = Manifest.permission.BIND_SCREENING_SERVICE;
        }};
        inputResult = new CallFilteringResult.Builder()
                .setShouldAllowCall(true)
                .setShouldReject(false)
                .setShouldAddToCallLog(true)
                .setShouldShowNotification(true)
                .build();

        when(mCallsManager.getCurrentUserHandle()).thenReturn(UserHandle.CURRENT);
        when(mCall.getId()).thenReturn(CALL_ID);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mContext.getSystemService(TelecomManager.class))
                .thenReturn(mTelecomManager);
        when(mTelecomManager.getSystemDialerPackage()).thenReturn(PKG_NAME);
        when(mAppLabelProxy.getAppLabel(PKG_NAME)).thenReturn(APP_NAME);
        when(mParcelableCallUtilsConverter.toParcelableCall(
                eq(mCall), anyBoolean(), eq(mPhoneAccountRegistrar))).thenReturn(null);
        when(mContext.bindServiceAsUser(nullable(Intent.class), nullable(ServiceConnection.class),
                anyInt(), eq(UserHandle.CURRENT))).thenReturn(true);
        when(mPackageManager.queryIntentServicesAsUser(nullable(Intent.class), anyInt(), anyInt()))
                .thenReturn(Collections.singletonList(mResolveInfo));
        doReturn(mCallScreeningService).when(mBinder).queryLocalInterface(anyString());
    }

    @SmallTest
    @Test
    public void testNoPackageName() throws Exception {
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, null,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        assertEquals(PASS_RESULT,
                filter.startFilterLookup(inputResult).toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testContextFailToBind() throws Exception {
        when(mContext.bindServiceAsUser(nullable(Intent.class), nullable(ServiceConnection.class),
                anyInt(), eq(UserHandle.CURRENT))).thenReturn(false);
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        assertEquals(PASS_RESULT,
                filter.startFilterLookup(inputResult).toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testNoResolveEntries() throws Exception {
        when(mPackageManager.queryIntentServicesAsUser(nullable(Intent.class), anyInt(), anyInt()))
                .thenReturn(Collections.emptyList());
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        assertEquals(PASS_RESULT,
                filter.startFilterLookup(inputResult).toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testBadResolveEntry() throws Exception {
        mResolveInfo.serviceInfo = null;
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        assertEquals(PASS_RESULT,
                filter.startFilterLookup(inputResult).toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testNoBindingCondition() {
        // Make sure there will be no binding if the package has no READ_CONTACT permission and
        // contact exist.
        when(mPackageManager.checkPermission(Manifest.permission.READ_CONTACTS, PKG_NAME))
                .thenReturn(PackageManager.PERMISSION_DENIED);
        when(mContext.bindServiceAsUser(nullable(Intent.class), nullable(ServiceConnection.class),
                anyInt(), eq(UserHandle.CURRENT))).thenThrow(new SecurityException());
        inputResult.contactExists = true;
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_USER_CHOSEN, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        filter.startFilterLookup(inputResult);
    }

    @SmallTest
    @Test
    public void testBindingCondition() {
        // Make sure there will be binding if the package has READ_CONTACT permission and contact
        // exist.
        inputResult.contactExists = true;
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        filter.startFilterLookup(inputResult);
        ServiceConnection connection = verifyBindingIntent();
        connection.onServiceDisconnected(COMPONENT_NAME);
    }

    @SmallTest
    @Test
    public void testUnbindingException() {
        // Make sure that exceptions when unbinding won't make the device crash.
        doThrow(new IllegalArgumentException()).when(mContext)
                .unbindService(nullable(ServiceConnection.class));
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        filter.startFilterLookup(inputResult);
        filter.unbindCallScreeningService();
    }

    @SmallTest
    @Test
    public void testAllowCall() throws Exception {
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        CompletionStage<CallFilteringResult> resultFuture = filter.startFilterLookup(inputResult);

        ServiceConnection serviceConnection = verifyBindingIntent();

        serviceConnection.onServiceConnected(COMPONENT_NAME, mBinder);
        ICallScreeningAdapter csAdapter = getCallScreeningAdapter();
        csAdapter.allowCall(CALL_ID);
        assertEquals(PASS_RESULT,
                resultFuture.toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
        serviceConnection.onServiceDisconnected(COMPONENT_NAME);
    }

    @SmallTest
    @Test
    public void testDisallowCall() throws Exception {
        CallFilteringResult expectedResult = new CallFilteringResult.Builder()
                .setShouldAllowCall(false)
                .setShouldReject(true)
                .setShouldSilence(false)
                .setShouldAddToCallLog(true)
                .setShouldShowNotification(true)
                .setCallBlockReason(CallLog.Calls.BLOCK_REASON_CALL_SCREENING_SERVICE)
                .setCallScreeningAppName(APP_NAME)
                .setCallScreeningComponentName(COMPONENT_NAME.flattenToString())
                .build();
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        CompletionStage<CallFilteringResult> resultFuture = filter.startFilterLookup(inputResult);

        ServiceConnection serviceConnection = verifyBindingIntent();

        serviceConnection.onServiceConnected(COMPONENT_NAME, mBinder);
        ICallScreeningAdapter csAdapter = getCallScreeningAdapter();
        csAdapter.disallowCall(CALL_ID,
                true, // shouldReject
                true, //shouldAddToCallLog
                true, // shouldShowNotification
                COMPONENT_NAME);
        assertEquals(expectedResult,
                resultFuture.toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
        serviceConnection.onServiceDisconnected(COMPONENT_NAME);
    }

    @SmallTest
    @Test
    public void testSilenceCall() throws Exception {
        CallFilteringResult expectedResult = new CallFilteringResult.Builder()
                .setShouldAllowCall(true)
                .setShouldReject(false)
                .setShouldSilence(true)
                .setShouldAddToCallLog(true)
                .setShouldShowNotification(true)
                .build();
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        CompletionStage<CallFilteringResult> resultFuture = filter.startFilterLookup(inputResult);

        ServiceConnection serviceConnection = verifyBindingIntent();

        serviceConnection.onServiceConnected(COMPONENT_NAME, mBinder);
        ICallScreeningAdapter csAdapter = getCallScreeningAdapter();
        csAdapter.silenceCall(CALL_ID);
        assertEquals(expectedResult,
                resultFuture.toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));

        serviceConnection.onServiceDisconnected(COMPONENT_NAME);
    }

    @SmallTest
    @Test
    public void testScreenCallFurther() throws Exception {
        CallFilteringResult expectedResult = new CallFilteringResult.Builder()
                .setShouldAllowCall(true)
                .setShouldReject(false)
                .setShouldSilence(false)
                .setShouldScreenViaAudio(true)
                .setCallScreeningAppName(APP_NAME)
                .build();
        CallScreeningServiceFilter filter = new CallScreeningServiceFilter(mCall, PKG_NAME,
                CallScreeningServiceFilter.PACKAGE_TYPE_DEFAULT_DIALER, mContext, mCallsManager,
                mAppLabelProxy, mParcelableCallUtilsConverter);
        CompletionStage<CallFilteringResult> resultFuture = filter.startFilterLookup(inputResult);

        ServiceConnection serviceConnection = verifyBindingIntent();

        serviceConnection.onServiceConnected(COMPONENT_NAME, mBinder);
        ICallScreeningAdapter csAdapter = getCallScreeningAdapter();
        csAdapter.screenCallFurther(CALL_ID);
        assertEquals(expectedResult,
                resultFuture.toCompletableFuture().get(
                        CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT,
                        TimeUnit.MILLISECONDS));
        serviceConnection.onServiceDisconnected(COMPONENT_NAME);
    }

    private ServiceConnection verifyBindingIntent() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<ServiceConnection> serviceCaptor = ArgumentCaptor
                .forClass(ServiceConnection.class);
        verify(mContext, timeout(CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT))
                .bindServiceAsUser(intentCaptor.capture(), serviceCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE),
                eq(UserHandle.CURRENT));

        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(CallScreeningService.SERVICE_INTERFACE, capturedIntent.getAction());
        assertEquals(mResolveInfo.serviceInfo.packageName, capturedIntent.getPackage());
        assertEquals(new ComponentName(mResolveInfo.serviceInfo.packageName,
                mResolveInfo.serviceInfo.name), capturedIntent.getComponent());

        return serviceCaptor.getValue();
    }

    private ICallScreeningAdapter getCallScreeningAdapter() throws Exception {
        ArgumentCaptor<ICallScreeningAdapter> captor =
                ArgumentCaptor.forClass(ICallScreeningAdapter.class);
        verify(mCallScreeningService,
                timeout(CallScreeningServiceFilter.CALL_SCREENING_FILTER_TIMEOUT))
                .screenCall(captor.capture(), nullable(ParcelableCall.class));
        return captor.getValue();
    }
}
