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
 * limitations under the License
 */

package com.android.server.telecom.tests;

import static com.android.server.telecom.InCallController.IN_CALL_SERVICE_NOTIFICATION_ID;
import static com.android.server.telecom.InCallController.NOTIFICATION_TAG;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.AppOpsManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.UiModeManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.res.Resources;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.UserHandle;
import android.telecom.CallAudioState;
import android.telecom.InCallService;
import android.telecom.ParcelableCall;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;

import com.android.internal.telecom.IInCallAdapter;
import com.android.internal.telecom.IInCallService;
import com.android.server.telecom.Analytics;
import com.android.server.telecom.BluetoothHeadsetProxy;
import com.android.server.telecom.Call;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.CarModeTracker;
import com.android.server.telecom.ClockProxy;
import com.android.server.telecom.DefaultDialerCache;
import com.android.server.telecom.EmergencyCallHelper;
import com.android.server.telecom.InCallController;
import com.android.server.telecom.PhoneAccountRegistrar;
import com.android.server.telecom.R;
import com.android.server.telecom.RoleManagerAdapter;
import com.android.server.telecom.SystemStateHelper;
import com.android.server.telecom.TelecomSystem;
import com.android.server.telecom.Timeouts;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

@RunWith(JUnit4.class)
public class InCallControllerTests extends TelecomTestCase {
    @Mock CallsManager mMockCallsManager;
    @Mock PhoneAccountRegistrar mMockPhoneAccountRegistrar;
    @Mock BluetoothHeadsetProxy mMockBluetoothHeadset;
    @Mock SystemStateHelper mMockSystemStateHelper;
    @Mock PackageManager mMockPackageManager;
    @Mock Call mMockCall;
    @Mock Resources mMockResources;
    @Mock AppOpsManager mMockAppOpsManager;
    @Mock MockContext mMockContext;
    @Mock Timeouts.Adapter mTimeoutsAdapter;
    @Mock DefaultDialerCache mDefaultDialerCache;
    @Mock RoleManagerAdapter mMockRoleManagerAdapter;
    @Mock ClockProxy mClockProxy;
    @Mock Analytics.CallInfoImpl mCallInfo;
    @Mock NotificationManager mNotificationManager;

    private static final int CURRENT_USER_ID = 900973;
    private static final String DEF_PKG = "defpkg";
    private static final String DEF_CLASS = "defcls";
    private static final int DEF_UID = 1;
    private static final String SYS_PKG = "syspkg";
    private static final String SYS_CLASS = "syscls";
    private static final int SYS_UID = 2;
    private static final String COMPANION_PKG = "cpnpkg";
    private static final String COMPANION_CLASS = "cpncls";
    private static final int COMPANION_UID = 3;
    private static final String CAR_PKG = "carpkg";
    private static final String CAR2_PKG = "carpkg2";
    private static final String CAR_CLASS = "carcls";
    private static final String CAR2_CLASS = "carcls";
    private static final int CAR_UID = 4;
    private static final int CAR2_UID = 5;
    private static final String NONUI_PKG = "nonui_pkg";
    private static final String NONUI_CLASS = "nonui_cls";
    private static final int NONUI_UID = 6;

    private static final PhoneAccountHandle PA_HANDLE =
            new PhoneAccountHandle(new ComponentName("pa_pkg", "pa_cls"), "pa_id");

    private UserHandle mUserHandle = UserHandle.of(CURRENT_USER_ID);
    private InCallController mInCallController;
    private TelecomSystem.SyncRoot mLock = new TelecomSystem.SyncRoot() {};
    private EmergencyCallHelper mEmergencyCallHelper;

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        when(mMockCall.getAnalytics()).thenReturn(new Analytics.CallInfo());
        doReturn(mMockResources).when(mMockContext).getResources();
        doReturn(mMockAppOpsManager).when(mMockContext).getSystemService(AppOpsManager.class);
        doReturn(SYS_PKG).when(mMockResources).getString(
                com.android.internal.R.string.config_defaultDialer);
        doReturn(SYS_CLASS).when(mMockResources).getString(R.string.incall_default_class);
        doReturn(true).when(mMockResources).getBoolean(R.bool.grant_location_permission_enabled);
        when(mDefaultDialerCache.getSystemDialerApplication()).thenReturn(SYS_PKG);
        when(mDefaultDialerCache.getSystemDialerComponent()).thenReturn(
                new ComponentName(SYS_PKG, SYS_CLASS));
        mEmergencyCallHelper = new EmergencyCallHelper(mMockContext, mDefaultDialerCache,
                mTimeoutsAdapter);
        when(mMockCallsManager.getRoleManagerAdapter()).thenReturn(mMockRoleManagerAdapter);
        when(mMockContext.getSystemService(eq(Context.NOTIFICATION_SERVICE)))
                .thenReturn(mNotificationManager);
        mInCallController = new InCallController(mMockContext, mLock, mMockCallsManager,
                mMockSystemStateHelper, mDefaultDialerCache, mTimeoutsAdapter,
                mEmergencyCallHelper, new CarModeTracker(), mClockProxy);
        // Companion Apps don't have CONTROL_INCALL_EXPERIENCE permission.
        doAnswer(invocation -> {
            int uid = invocation.getArgument(0);
            switch (uid) {
                case DEF_UID:
                    return new String[] { DEF_PKG };
                case SYS_UID:
                    return new String[] { SYS_PKG };
                case COMPANION_UID:
                    return new String[] { COMPANION_PKG };
                case CAR_UID:
                    return new String[] { CAR_PKG };
                case CAR2_UID:
                    return new String[] { CAR2_PKG };
                case NONUI_UID:
                    return new String[] { NONUI_PKG };
            }
            return null;
        }).when(mMockPackageManager).getPackagesForUid(anyInt());
        when(mMockPackageManager.checkPermission(
                matches(Manifest.permission.CONTROL_INCALL_EXPERIENCE),
                matches(COMPANION_PKG))).thenReturn(PackageManager.PERMISSION_DENIED);
        when(mMockPackageManager.checkPermission(
                matches(Manifest.permission.CONTROL_INCALL_EXPERIENCE),
                matches(CAR_PKG))).thenReturn(PackageManager.PERMISSION_GRANTED);
        when(mMockPackageManager.checkPermission(
                matches(Manifest.permission.CONTROL_INCALL_EXPERIENCE),
                matches(CAR2_PKG))).thenReturn(PackageManager.PERMISSION_GRANTED);
        when(mMockPackageManager.checkPermission(
                matches(Manifest.permission.CONTROL_INCALL_EXPERIENCE),
                matches(NONUI_PKG))).thenReturn(PackageManager.PERMISSION_GRANTED);
        when(mMockCallsManager.getAudioState()).thenReturn(new CallAudioState(false, 0, 0));
    }

    @Override
    @After
    public void tearDown() throws Exception {
        mInCallController.getHandler().removeCallbacksAndMessages(null);
        waitForHandlerAction(mInCallController.getHandler(), 1000);
        super.tearDown();
    }

    @MediumTest
    @Test
    public void testBindToService_NoServicesFound_IncomingCall() throws Exception {
        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(true);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mTimeoutsAdapter.getEmergencyCallbackWindowMillis(any(ContentResolver.class)))
                .thenReturn(300_000L);

        setupMockPackageManager(false /* default */, true /* system */, false /* external calls */);
        mInCallController.bindToServices(mMockCall);

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(SYS_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(SYS_CLASS, bindIntent.getComponent().getClassName());
        assertNull(bindIntent.getExtras());
    }

    @MediumTest
    @Test
    public void testBindToService_NoServicesFound_OutgoingCall() throws Exception {
        Bundle callExtras = new Bundle();
        callExtras.putBoolean("whatever", true);

        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.getTargetPhoneAccount()).thenReturn(PA_HANDLE);
        when(mMockCall.getIntentExtras()).thenReturn(callExtras);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mTimeoutsAdapter.getEmergencyCallbackWindowMillis(any(ContentResolver.class)))
                .thenReturn(300_000L);

        Intent queryIntent = new Intent(InCallService.SERVICE_INTERFACE);
        setupMockPackageManager(false /* default */, true /* system */, false /* external calls */);
        mInCallController.bindToServices(mMockCall);

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(SYS_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(SYS_CLASS, bindIntent.getComponent().getClassName());
        assertEquals(PA_HANDLE, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE));
        assertEquals(callExtras, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS));
    }

    @MediumTest
    @Test
    public void testBindToService_DefaultDialer_NoEmergency() throws Exception {
        Bundle callExtras = new Bundle();
        callExtras.putBoolean("whatever", true);

        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.getTargetPhoneAccount()).thenReturn(PA_HANDLE);
        when(mMockCall.getIntentExtras()).thenReturn(callExtras);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID))
                .thenReturn(DEF_PKG);
        when(mMockContext.bindServiceAsUser(any(Intent.class), any(ServiceConnection.class),
                anyInt(), eq(UserHandle.CURRENT))).thenReturn(true);

        setupMockPackageManager(true /* default */, true /* system */, false /* external calls */);
        mInCallController.bindToServices(mMockCall);

        // Query for the different InCallServices
        ArgumentCaptor<Intent> queryIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPackageManager, times(4)).queryIntentServicesAsUser(
                queryIntentCaptor.capture(),
                eq(PackageManager.GET_META_DATA), eq(CURRENT_USER_ID));

        // Verify call for default dialer InCallService
        assertEquals(DEF_PKG, queryIntentCaptor.getAllValues().get(0).getPackage());
        // Verify call for car-mode InCallService
        assertEquals(null, queryIntentCaptor.getAllValues().get(1).getPackage());
        // Verify call for non-UI InCallServices
        assertEquals(null, queryIntentCaptor.getAllValues().get(2).getPackage());

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(DEF_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(DEF_CLASS, bindIntent.getComponent().getClassName());
        assertEquals(PA_HANDLE, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE));
        assertEquals(callExtras, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS));
    }

    @MediumTest
    @Test
    public void testBindToService_SystemDialer_Emergency() throws Exception {
        Bundle callExtras = new Bundle();
        callExtras.putBoolean("whatever", true);

        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(true);
        when(mMockCall.isEmergencyCall()).thenReturn(true);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.getTargetPhoneAccount()).thenReturn(PA_HANDLE);
        when(mMockCall.getIntentExtras()).thenReturn(callExtras);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID))
                .thenReturn(DEF_PKG);
        when(mMockContext.bindServiceAsUser(any(Intent.class), any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT))).thenReturn(true);
        when(mTimeoutsAdapter.getEmergencyCallbackWindowMillis(any(ContentResolver.class)))
                .thenReturn(300_000L);

        setupMockPackageManager(true /* default */, true /* system */, false /* external calls */);
        setupMockPackageManagerLocationPermission(SYS_PKG, false /* granted */);

        mInCallController.bindToServices(mMockCall);

        // Query for the different InCallServices
        ArgumentCaptor<Intent> queryIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPackageManager, times(4)).queryIntentServicesAsUser(
                queryIntentCaptor.capture(),
                eq(PackageManager.GET_META_DATA), eq(CURRENT_USER_ID));

        // Verify call for default dialer InCallService
        assertEquals(DEF_PKG, queryIntentCaptor.getAllValues().get(0).getPackage());
        // Verify call for car-mode InCallService
        assertEquals(null, queryIntentCaptor.getAllValues().get(1).getPackage());
        // Verify call for non-UI InCallServices
        assertEquals(null, queryIntentCaptor.getAllValues().get(2).getPackage());

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(SYS_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(SYS_CLASS, bindIntent.getComponent().getClassName());
        assertEquals(PA_HANDLE, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE));
        assertEquals(callExtras, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS));

        verify(mMockPackageManager).grantRuntimePermission(eq(SYS_PKG),
                eq(Manifest.permission.ACCESS_FINE_LOCATION), eq(mUserHandle));

        // Pretend that the call has gone away.
        when(mMockCallsManager.getCalls()).thenReturn(Collections.emptyList());
        mInCallController.onCallRemoved(mMockCall);
        waitForHandlerAction(new Handler(Looper.getMainLooper()), TelecomSystemTest.TEST_TIMEOUT);

        verify(mMockPackageManager).revokeRuntimePermission(eq(SYS_PKG),
                eq(Manifest.permission.ACCESS_FINE_LOCATION), eq(mUserHandle));
    }

    /**
     * This test verifies the behavior of Telecom when the system dialer crashes on binding and must
     * be restarted.  Specifically, it ensures when the system dialer crashes we revoke the runtime
     * location permission, and when it restarts we re-grant the permission.
     * @throws Exception
     */
    @MediumTest
    @Test
    public void testBindToService_SystemDialer_Crash() throws Exception {
        Bundle callExtras = new Bundle();
        callExtras.putBoolean("whatever", true);

        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(true);
        when(mMockCall.isEmergencyCall()).thenReturn(true);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.getTargetPhoneAccount()).thenReturn(PA_HANDLE);
        when(mMockCall.getIntentExtras()).thenReturn(callExtras);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID))
                .thenReturn(DEF_PKG);
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        when(mMockContext.bindServiceAsUser(any(Intent.class), serviceConnectionCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT))).thenReturn(true);
        when(mTimeoutsAdapter.getEmergencyCallbackWindowMillis(any(ContentResolver.class)))
                .thenReturn(300_000L);

        setupMockPackageManager(true /* default */, true /* system */, false /* external calls */);
        setupMockPackageManagerLocationPermission(SYS_PKG, false /* granted */);

        mInCallController.bindToServices(mMockCall);

        // Query for the different InCallServices
        ArgumentCaptor<Intent> queryIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPackageManager, times(4)).queryIntentServicesAsUser(
                queryIntentCaptor.capture(),
                eq(PackageManager.GET_META_DATA), eq(CURRENT_USER_ID));

        // Verify call for default dialer InCallService
        assertEquals(DEF_PKG, queryIntentCaptor.getAllValues().get(0).getPackage());
        // Verify call for car-mode InCallService
        assertEquals(null, queryIntentCaptor.getAllValues().get(1).getPackage());
        // Verify call for non-UI InCallServices
        assertEquals(null, queryIntentCaptor.getAllValues().get(2).getPackage());

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(SYS_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(SYS_CLASS, bindIntent.getComponent().getClassName());
        assertEquals(PA_HANDLE, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE));
        assertEquals(callExtras, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS));

        verify(mMockPackageManager).grantRuntimePermission(eq(SYS_PKG),
                eq(Manifest.permission.ACCESS_FINE_LOCATION), eq(mUserHandle));

        // Emulate a crash in the system dialer; we'll use the captured service connection to signal
        // to InCallController that the dialer died.
        ServiceConnection serviceConnection = serviceConnectionCaptor.getValue();
        serviceConnection.onServiceDisconnected(bindIntent.getComponent());

        // We expect that the permission is revoked at this point.
        verify(mMockPackageManager).revokeRuntimePermission(eq(SYS_PKG),
                eq(Manifest.permission.ACCESS_FINE_LOCATION), eq(mUserHandle));

        // Now, we expect to auto-rebind to the system dialer (verify 2 times since this is the
        // second binding).
        verify(mMockContext, times(2)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        // Verify we were re-granted the runtime permission.
        verify(mMockPackageManager, times(2)).grantRuntimePermission(eq(SYS_PKG),
                eq(Manifest.permission.ACCESS_FINE_LOCATION), eq(mUserHandle));
    }

    @MediumTest
    @Test
    public void testBindToService_DefaultDialer_FallBackToSystem() throws Exception {
        Bundle callExtras = new Bundle();
        callExtras.putBoolean("whatever", true);

        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCallsManager.getCalls()).thenReturn(Collections.singletonList(mMockCall));
        when(mMockCallsManager.getAudioState()).thenReturn(null);
        when(mMockCallsManager.canAddCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.getTargetPhoneAccount()).thenReturn(PA_HANDLE);
        when(mMockCall.getIntentExtras()).thenReturn(callExtras);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mMockCall.getConferenceableCalls()).thenReturn(Collections.emptyList());
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID))
                .thenReturn(DEF_PKG);
        when(mMockContext.bindServiceAsUser(
                any(Intent.class), any(ServiceConnection.class), anyInt(), any(UserHandle.class)))
                .thenReturn(true);

        setupMockPackageManager(true /* default */, true /* system */, false /* external calls */);
        mInCallController.bindToServices(mMockCall);

        // Query for the different InCallServices
        ArgumentCaptor<Intent> queryIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPackageManager, times(4)).queryIntentServicesAsUser(
                queryIntentCaptor.capture(),
                eq(PackageManager.GET_META_DATA), eq(CURRENT_USER_ID));

        // Verify call for default dialer InCallService
        assertEquals(DEF_PKG, queryIntentCaptor.getAllValues().get(0).getPackage());
        // Verify call for car-mode InCallService
        assertEquals(null, queryIntentCaptor.getAllValues().get(1).getPackage());
        // Verify call for non-UI InCallServices
        assertEquals(null, queryIntentCaptor.getAllValues().get(2).getPackage());

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                serviceConnectionCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(DEF_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(DEF_CLASS, bindIntent.getComponent().getClassName());
        assertEquals(PA_HANDLE, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE));
        assertEquals(callExtras, bindIntent.getExtras().getParcelable(
                TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS));

        // We have a ServiceConnection for the default dialer, lets start the connection, and then
        // simulate a crash so that we fallback to system.
        ServiceConnection serviceConnection = serviceConnectionCaptor.getValue();
        ComponentName defDialerComponentName = new ComponentName(DEF_PKG, DEF_CLASS);
        IBinder mockBinder = mock(IBinder.class);
        IInCallService mockInCallService = mock(IInCallService.class);
        when(mockBinder.queryLocalInterface(anyString())).thenReturn(mockInCallService);


        // Start the connection with IInCallService
        serviceConnection.onServiceConnected(defDialerComponentName, mockBinder);
        verify(mockInCallService).setInCallAdapter(any(IInCallAdapter.class));

        // Now crash the damn thing!
        serviceConnection.onServiceDisconnected(defDialerComponentName);

        ArgumentCaptor<Intent> bindIntentCaptor2 = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(2)).bindServiceAsUser(
                bindIntentCaptor2.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        bindIntent = bindIntentCaptor2.getValue();
        assertEquals(SYS_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(SYS_CLASS, bindIntent.getComponent().getClassName());
    }

    @Test
    public void testBindToService_NullBinding_FallBackToSystem() throws Exception {
        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.targetSdkVersion = Build.VERSION_CODES.R;
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockCall.getAnalytics()).thenReturn(mCallInfo);
        when(mMockContext.bindServiceAsUser(
                any(Intent.class), any(ServiceConnection.class), anyInt(), any(UserHandle.class)))
                .thenReturn(true);
        when(mMockContext.getApplicationInfo()).thenReturn(applicationInfo);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID)).thenReturn(DEF_PKG);

        setupMockPackageManager(true /* default */, true /* system */, false /* external calls */);
        mInCallController.bindToServices(mMockCall);

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                serviceConnectionCaptor.capture(),
                anyInt(),
                any(UserHandle.class));
        ServiceConnection serviceConnection = serviceConnectionCaptor.getValue();
        ComponentName defDialerComponentName = new ComponentName(DEF_PKG, DEF_CLASS);
        ComponentName sysDialerComponentName = new ComponentName(SYS_PKG, SYS_CLASS);

        IBinder mockBinder = mock(IBinder.class);
        IInCallService mockInCallService = mock(IInCallService.class);
        when(mockBinder.queryLocalInterface(anyString())).thenReturn(mockInCallService);

        serviceConnection.onServiceConnected(defDialerComponentName, mockBinder);
        // verify(mockInCallService).setInCallAdapter(any(IInCallAdapter.class));
        serviceConnection.onNullBinding(defDialerComponentName);

        verify(mNotificationManager).notify(eq(NOTIFICATION_TAG),
                eq(IN_CALL_SERVICE_NOTIFICATION_ID), any(Notification.class));
        verify(mCallInfo).addInCallService(eq(defDialerComponentName.flattenToShortString()),
                anyInt(), anyLong(), eq(true));

        ArgumentCaptor<Intent> bindIntentCaptor2 = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(2)).bindServiceAsUser(
                bindIntentCaptor2.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));
        assertEquals(sysDialerComponentName, bindIntentCaptor2.getValue().getComponent());
    }

    /**
     * Ensures that the {@link InCallController} will bind to an {@link InCallService} which
     * supports external calls.
     */
    @MediumTest
    @Test
    public void testBindToService_IncludeExternal() throws Exception {
        setupMocks(true /* isExternalCall */);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */);
        mInCallController.bindToServices(mMockCall);

        // Query for the different InCallServices
        ArgumentCaptor<Intent> queryIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockPackageManager, times(4)).queryIntentServicesAsUser(
                queryIntentCaptor.capture(),
                eq(PackageManager.GET_META_DATA), eq(CURRENT_USER_ID));

        // Verify call for default dialer InCallService
        assertEquals(DEF_PKG, queryIntentCaptor.getAllValues().get(0).getPackage());
        // Verify call for car-mode InCallService
        assertEquals(null, queryIntentCaptor.getAllValues().get(1).getPackage());
        // Verify call for non-UI InCallServices
        assertEquals(null, queryIntentCaptor.getAllValues().get(2).getPackage());

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        Intent bindIntent = bindIntentCaptor.getValue();
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(DEF_PKG, bindIntent.getComponent().getPackageName());
        assertEquals(DEF_CLASS, bindIntent.getComponent().getClassName());
    }

    /**
     * Make sure that if a call goes away before the in-call service finishes binding and another
     * call gets connected soon after, the new call will still be sent to the in-call service.
     */
    @MediumTest
    @Test
    public void testUnbindDueToCallDisconnect() throws Exception {
        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(true);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID)).thenReturn(DEF_PKG);
        when(mMockContext.bindServiceAsUser(nullable(Intent.class),
                nullable(ServiceConnection.class), anyInt(), nullable(UserHandle.class)))
                .thenReturn(true);
        when(mTimeoutsAdapter.getCallRemoveUnbindInCallServicesDelay(
                nullable(ContentResolver.class))).thenReturn(500L);

        when(mMockCallsManager.getCalls()).thenReturn(Collections.singletonList(mMockCall));
        setupMockPackageManager(true /* default */, true /* system */, false /* external calls */);
        mInCallController.bindToServices(mMockCall);

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                serviceConnectionCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        // Pretend that the call has gone away.
        when(mMockCallsManager.getCalls()).thenReturn(Collections.emptyList());
        mInCallController.onCallRemoved(mMockCall);

        // Start the connection, make sure we don't unbind, and make sure that we don't send
        // anything to the in-call service yet.
        ServiceConnection serviceConnection = serviceConnectionCaptor.getValue();
        ComponentName defDialerComponentName = new ComponentName(DEF_PKG, DEF_CLASS);
        IBinder mockBinder = mock(IBinder.class);
        IInCallService mockInCallService = mock(IInCallService.class);
        when(mockBinder.queryLocalInterface(anyString())).thenReturn(mockInCallService);

        serviceConnection.onServiceConnected(defDialerComponentName, mockBinder);
        verify(mockInCallService).setInCallAdapter(nullable(IInCallAdapter.class));
        verify(mMockContext, never()).unbindService(serviceConnection);
        verify(mockInCallService, never()).addCall(any(ParcelableCall.class));

        // Now, we add in the call again and make sure that it's sent to the InCallService.
        when(mMockCallsManager.getCalls()).thenReturn(Collections.singletonList(mMockCall));
        mInCallController.onCallAdded(mMockCall);
        verify(mockInCallService).addCall(any(ParcelableCall.class));
    }

    /**
     * Ensures that the {@link InCallController} will bind to an {@link InCallService} which
     * supports third party car mode ui calls
     */
    @MediumTest
    @Test
    public void testBindToService_CarModeUI() throws Exception {
        setupMocks(true /* isExternalCall */);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */);

        // Enable car mode
        when(mMockSystemStateHelper.isCarMode()).thenReturn(true);
        mInCallController.handleCarModeChange(UiModeManager.DEFAULT_PRIORITY, CAR_PKG, true);

        // Now bind; we should only bind to one app.
        mInCallController.bindToServices(mMockCall);

        // Bind InCallServices
        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));
        // Verify bind car mode ui
        assertEquals(1, bindIntentCaptor.getAllValues().size());
        verifyBinding(bindIntentCaptor, 0, CAR_PKG, CAR_CLASS);
    }

    @MediumTest
    @Test
    public void testNoBindToInvalidService_CarModeUI() throws Exception {
        setupMocks(true /* isExternalCall */);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */);
        mInCallController.bindToServices(mMockCall);

        when(mMockPackageManager.checkPermission(
                matches(Manifest.permission.CONTROL_INCALL_EXPERIENCE),
                matches(CAR_PKG))).thenReturn(PackageManager.PERMISSION_DENIED);
        // Enable car mode
        when(mMockSystemStateHelper.isCarMode()).thenReturn(true);

        // Register the fact that the invalid app entered car mode.
        mInCallController.handleCarModeChange(UiModeManager.DEFAULT_PRIORITY, CAR_PKG, true);

        // Bind InCallServices
        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));
        // Verify bind to default package, instead of the invalid car mode ui.
        assertEquals(1, bindIntentCaptor.getAllValues().size());
        verifyBinding(bindIntentCaptor, 0, DEF_PKG, DEF_CLASS);
    }

    @MediumTest
    @Test
    public void testSanitizeContactName() throws Exception {
        setupMocks(false /* isExternalCall */);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */);
        when(mMockPackageManager.checkPermission(
                matches(Manifest.permission.READ_CONTACTS),
                matches(DEF_PKG))).thenReturn(PackageManager.PERMISSION_DENIED);
        when(mMockCall.getName()).thenReturn("evil");

        mInCallController.bindToServices(mMockCall);

        // Bind InCallServices
        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                serviceConnectionCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));
        assertEquals(1, bindIntentCaptor.getAllValues().size());
        verifyBinding(bindIntentCaptor, 0, DEF_PKG, DEF_CLASS);

        IInCallService.Stub mockInCallServiceStub = mock(IInCallService.Stub.class);
        IInCallService mockInCallService = mock(IInCallService.class);
        when(mockInCallServiceStub.queryLocalInterface(anyString())).thenReturn(mockInCallService);
        serviceConnectionCaptor.getValue().onServiceConnected(new ComponentName(DEF_PKG, DEF_CLASS),
                mockInCallServiceStub);

        mInCallController.onCallAdded(mMockCall);
        ArgumentCaptor<ParcelableCall> parcelableCallCaptor =
                ArgumentCaptor.forClass(ParcelableCall.class);
        verify(mockInCallService).addCall(parcelableCallCaptor.capture());
        assertTrue(TextUtils.isEmpty(parcelableCallCaptor.getValue().getContactDisplayName()));
    }

    /**
     * Ensures that the {@link InCallController} will bind to a higher priority car mode service
     * when one becomes available.
     */
    @MediumTest
    @Test
    public void testCarmodeRebindHigherPriority() throws Exception {
        setupMocks(true /* isExternalCall */);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */);
        // Bind to default dialer.
        mInCallController.bindToServices(mMockCall);

        // Enable car mode and enter car mode at default priority.
        when(mMockSystemStateHelper.isCarMode()).thenReturn(true);
        mInCallController.handleCarModeChange(UiModeManager.DEFAULT_PRIORITY, CAR_PKG, true);

        // And change to the second car mode app.
        mInCallController.handleCarModeChange(100, CAR2_PKG, true);

        // Exit car mode at higher priority.
        mInCallController.handleCarModeChange(100, CAR2_PKG, false);

        // Bind InCallServices
        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(4)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));
        // Verify bind car mode ui
        assertEquals(4, bindIntentCaptor.getAllValues().size());

        // Should have first bound to the default dialer.
        verifyBinding(bindIntentCaptor, 0, DEF_PKG, DEF_CLASS);

        // Should have next bound to the car mode app.
        verifyBinding(bindIntentCaptor, 1, CAR_PKG, CAR_CLASS);

        // Finally, should have bound to the higher priority car mode app
        verifyBinding(bindIntentCaptor, 2, CAR2_PKG, CAR2_CLASS);

        // Should have rebound to the car mode app.
        verifyBinding(bindIntentCaptor, 3, CAR_PKG, CAR_CLASS);
    }

    public void verifyBinding(ArgumentCaptor<Intent> bindIntentCaptor, int i, String carPkg,
            String carClass) {
        Intent bindIntent = bindIntentCaptor.getAllValues().get(i);
        assertEquals(InCallService.SERVICE_INTERFACE, bindIntent.getAction());
        assertEquals(carPkg, bindIntent.getComponent().getPackageName());
        assertEquals(carClass, bindIntent.getComponent().getClassName());
    }

    /**
     * Make sure the InCallController completes its binding future when the in call service
     * finishes binding.
     */
    @MediumTest
    @Test
    public void testBindingFuture() throws Exception {
        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.isExternalCall()).thenReturn(false);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID)).thenReturn(DEF_PKG);
        when(mMockContext.bindServiceAsUser(nullable(Intent.class),
                nullable(ServiceConnection.class), anyInt(), nullable(UserHandle.class)))
                .thenReturn(true);
        when(mTimeoutsAdapter.getCallRemoveUnbindInCallServicesDelay(
                nullable(ContentResolver.class))).thenReturn(500L);

        when(mMockCallsManager.getCalls()).thenReturn(Collections.singletonList(mMockCall));
        setupMockPackageManager(true /* default */, true /* nonui */, true /* system */,
                false /* external calls */,
                false /* self mgd in default*/, false /* self mgd in car*/);
        mInCallController.bindToServices(mMockCall);

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mMockContext, times(2)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                serviceConnectionCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        CompletableFuture<Boolean> bindTimeout = mInCallController.getBindingFuture();

        assertFalse(bindTimeout.isDone());

        // Start the connection, make sure we don't unbind, and make sure that we don't send
        // anything to the in-call service yet.
        List<ServiceConnection> serviceConnections = serviceConnectionCaptor.getAllValues();
        List<Intent> intents = bindIntentCaptor.getAllValues();

        // Find the non-ui service and have it connect first.
        int nonUiIdx = findFirstIndexMatching(intents,
                i -> NONUI_PKG.equals(i.getComponent().getPackageName()));
        if (nonUiIdx < 0) {
            fail("Did not bind to non-ui incall");
        }

        {
            ComponentName nonUiComponentName = new ComponentName(NONUI_PKG, NONUI_CLASS);
            IBinder mockBinder = mock(IBinder.class);
            IInCallService mockInCallService = mock(IInCallService.class);
            when(mockBinder.queryLocalInterface(anyString())).thenReturn(mockInCallService);
            serviceConnections.get(nonUiIdx).onServiceConnected(nonUiComponentName, mockBinder);

            // Make sure the non-ui binding didn't trigger the future.
            assertFalse(bindTimeout.isDone());
        }

        int defDialerIdx = findFirstIndexMatching(intents,
                i -> DEF_PKG.equals(i.getComponent().getPackageName()));
        if (defDialerIdx < 0) {
            fail("Did not bind to default dialer incall");
        }

        ComponentName defDialerComponentName = new ComponentName(DEF_PKG, DEF_CLASS);
        IBinder mockBinder = mock(IBinder.class);
        IInCallService mockInCallService = mock(IInCallService.class);
        when(mockBinder.queryLocalInterface(anyString())).thenReturn(mockInCallService);

        serviceConnections.get(defDialerIdx).onServiceConnected(defDialerComponentName, mockBinder);
        verify(mockInCallService).setInCallAdapter(nullable(IInCallAdapter.class));

        // Make sure that the future completed without timing out.
        assertTrue(bindTimeout.getNow(false));
    }

    /**
     * Verify that if we go from a dialer which doesn't support self managed calls to a car mode
     * dialer that does support them, we will bind.
     */
    @MediumTest
    @Test
    public void testBindToService_SelfManagedCarModeUI() throws Exception {
        setupMocks(true /* isExternalCall */, true /* isSelfManaged*/);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */,
                false /* selfManagedInDefaultDialer */, true /* selfManagedInCarModeDialer */);

        // Bind; we should not bind to anything right now; the dialer does not support self
        // managed calls.
        mInCallController.bindToServices(mMockCall);

        // Bind InCallServices; make sure no binding took place.  InCallController handles not
        // binding initially, but the rebind (see next test case) will always happen.
        verify(mMockContext, never()).bindServiceAsUser(
                any(Intent.class),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        // Now switch to car mode.
        // Enable car mode and enter car mode at default priority.
        when(mMockSystemStateHelper.isCarMode()).thenReturn(true);
        mInCallController.handleCarModeChange(UiModeManager.DEFAULT_PRIORITY, CAR_PKG, true);

        ArgumentCaptor<Intent> bindIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                bindIntentCaptor.capture(),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));
        // Verify bind car mode ui
        assertEquals(1, bindIntentCaptor.getAllValues().size());
        verifyBinding(bindIntentCaptor, 0, CAR_PKG, CAR_CLASS);
    }

    /**
     * Verify that if we go from a dialer which doesn't support self managed calls to a car mode
     * dialer that does not support them, the calls are not sent to the call mode UI.
     */
    @MediumTest
    @Test
    public void testBindToService_SelfManagedNoCarModeUI() throws Exception {
        setupMocks(true /* isExternalCall */, true /* isSelfManaged*/);
        setupMockPackageManager(true /* default */, true /* system */, true /* external calls */,
                false /* selfManagedInDefaultDialer */, false /* selfManagedInCarModeDialer */);

        // Bind; we should not bind to anything right now; the dialer does not support self
        // managed calls.
        mInCallController.bindToServices(mMockCall);

        // Bind InCallServices; make sure no binding took place.
        verify(mMockContext, never()).bindServiceAsUser(
                any(Intent.class),
                any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        // Now switch to car mode.
        // Enable car mode and enter car mode at default priority.
        when(mMockSystemStateHelper.isCarMode()).thenReturn(true);
        mInCallController.handleCarModeChange(UiModeManager.DEFAULT_PRIORITY, CAR_PKG, true);

        // We currently will bind to the car-mode InCallService even if there are no calls available
        // for it.  Its not perfect, but it reflects the fact that the InCallController isn't
        // sophisticated enough to realize until its already bound whether there are in fact calls
        // which will be sent to it.
        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mMockContext, times(1)).bindServiceAsUser(
                any(Intent.class),
                serviceConnectionCaptor.capture(),
                eq(Context.BIND_AUTO_CREATE | Context.BIND_FOREGROUND_SERVICE
                        | Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS),
                eq(UserHandle.CURRENT));

        ServiceConnection serviceConnection = serviceConnectionCaptor.getValue();
        ComponentName defDialerComponentName = new ComponentName(DEF_PKG, DEF_CLASS);
        IBinder mockBinder = mock(IBinder.class);
        IInCallService mockInCallService = mock(IInCallService.class);
        when(mockBinder.queryLocalInterface(anyString())).thenReturn(mockInCallService);

        // Emulate successful connection.
        serviceConnection.onServiceConnected(defDialerComponentName, mockBinder);
        verify(mockInCallService).setInCallAdapter(any(IInCallAdapter.class));

        // We should not have gotten informed about any calls
        verify(mockInCallService, never()).addCall(any(ParcelableCall.class));
    }

    private void setupMocks(boolean isExternalCall) {
        setupMocks(isExternalCall, false /* isSelfManagedCall */);
    }

    private void setupMocks(boolean isExternalCall, boolean isSelfManagedCall) {
        when(mMockCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockCallsManager.isInEmergencyCall()).thenReturn(false);
        when(mMockCall.isIncoming()).thenReturn(false);
        when(mMockCall.getTargetPhoneAccount()).thenReturn(PA_HANDLE);
        when(mDefaultDialerCache.getDefaultDialerApplication(CURRENT_USER_ID)).thenReturn(DEF_PKG);
        when(mMockContext.bindServiceAsUser(any(Intent.class), any(ServiceConnection.class),
                anyInt(), eq(UserHandle.CURRENT))).thenReturn(true);
        when(mMockCall.isExternalCall()).thenReturn(isExternalCall);
        when(mMockCall.isSelfManaged()).thenReturn(isSelfManagedCall);
    }

    private ResolveInfo getDefResolveInfo(final boolean includeExternalCalls,
            final boolean includeSelfManagedCalls) {
        return new ResolveInfo() {{
            serviceInfo = new ServiceInfo();
            serviceInfo.packageName = DEF_PKG;
            serviceInfo.name = DEF_CLASS;
            serviceInfo.applicationInfo = new ApplicationInfo();
            serviceInfo.applicationInfo.uid = DEF_UID;
            serviceInfo.permission = Manifest.permission.BIND_INCALL_SERVICE;
            serviceInfo.metaData = new Bundle();
            serviceInfo.metaData.putBoolean(
                    TelecomManager.METADATA_IN_CALL_SERVICE_UI, true);
            if (includeExternalCalls) {
                serviceInfo.metaData.putBoolean(
                        TelecomManager.METADATA_INCLUDE_EXTERNAL_CALLS, true);
            }
            if (includeSelfManagedCalls) {
                serviceInfo.metaData.putBoolean(
                        TelecomManager.METADATA_INCLUDE_SELF_MANAGED_CALLS, true);
            }
        }};
    }

    private ResolveInfo getCarModeResolveinfo(final String packageName, final String className,
            final boolean includeExternalCalls, final boolean includeSelfManagedCalls) {
        return new ResolveInfo() {{
            serviceInfo = new ServiceInfo();
            serviceInfo.packageName = packageName;
            serviceInfo.name = className;
            serviceInfo.applicationInfo = new ApplicationInfo();
            if (CAR_PKG.equals(packageName)) {
                serviceInfo.applicationInfo.uid = CAR_UID;
            } else {
                serviceInfo.applicationInfo.uid = CAR2_UID;
            }
            serviceInfo.permission = Manifest.permission.BIND_INCALL_SERVICE;
            serviceInfo.metaData = new Bundle();
            serviceInfo.metaData.putBoolean(
                    TelecomManager.METADATA_IN_CALL_SERVICE_CAR_MODE_UI, true);
            if (includeExternalCalls) {
                serviceInfo.metaData.putBoolean(
                        TelecomManager.METADATA_INCLUDE_EXTERNAL_CALLS, true);
            }
            if (includeSelfManagedCalls) {
                serviceInfo.metaData.putBoolean(
                        TelecomManager.METADATA_INCLUDE_SELF_MANAGED_CALLS, true);
            }
        }};
    }

    private ResolveInfo getSysResolveinfo() {
        return new ResolveInfo() {{
            serviceInfo = new ServiceInfo();
            serviceInfo.packageName = SYS_PKG;
            serviceInfo.name = SYS_CLASS;
            serviceInfo.applicationInfo = new ApplicationInfo();
            serviceInfo.applicationInfo.uid = SYS_UID;
            serviceInfo.permission = Manifest.permission.BIND_INCALL_SERVICE;
        }};
    }

    private ResolveInfo getCompanionResolveinfo() {
        return new ResolveInfo() {{
            serviceInfo = new ServiceInfo();
            serviceInfo.packageName = COMPANION_PKG;
            serviceInfo.name = COMPANION_CLASS;
            serviceInfo.applicationInfo = new ApplicationInfo();
            serviceInfo.applicationInfo.uid = COMPANION_UID;
            serviceInfo.permission = Manifest.permission.BIND_INCALL_SERVICE;
        }};
    }

    private ResolveInfo getNonUiResolveinfo() {
        return new ResolveInfo() {{
            serviceInfo = new ServiceInfo();
            serviceInfo.packageName = NONUI_PKG;
            serviceInfo.name = NONUI_CLASS;
            serviceInfo.applicationInfo = new ApplicationInfo();
            serviceInfo.applicationInfo.uid = NONUI_UID;
            serviceInfo.enabled = true;
            serviceInfo.permission = Manifest.permission.BIND_INCALL_SERVICE;
        }};
    }

    private void setupMockPackageManager(final boolean useDefaultDialer,
            final boolean useSystemDialer, final boolean includeExternalCalls) {
        setupMockPackageManager(useDefaultDialer, false, useSystemDialer, includeExternalCalls,
                false /* self mgd */, false /* self mgd */);
    }

    private void setupMockPackageManager(final boolean useDefaultDialer,
            final boolean useSystemDialer, final boolean includeExternalCalls,
            final boolean includeSelfManagedCallsInDefaultDialer,
            final boolean includeSelfManagedCallsInCarModeDialer) {
        setupMockPackageManager(useDefaultDialer, false /* nonui */, useSystemDialer,
                includeExternalCalls, includeSelfManagedCallsInDefaultDialer,
                includeSelfManagedCallsInCarModeDialer);
    }

    private void setupMockPackageManager(final boolean useDefaultDialer,
            final boolean useNonUiInCalls,
            final boolean useSystemDialer, final boolean includeExternalCalls,
            final boolean includeSelfManagedCallsInDefaultDialer,
            final boolean includeSelfManagedCallsInCarModeDialer) {
        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                Object[] args = invocation.getArguments();
                Intent intent = (Intent) args[0];
                String packageName = intent.getPackage();
                ComponentName componentName = intent.getComponent();
                if (componentName != null) {
                    packageName = componentName.getPackageName();
                }
                LinkedList<ResolveInfo> resolveInfo = new LinkedList<ResolveInfo>();
                if (!TextUtils.isEmpty(packageName)) {
                    if (packageName.equals(DEF_PKG) && useDefaultDialer) {
                        resolveInfo.add(getDefResolveInfo(includeExternalCalls,
                                includeSelfManagedCallsInDefaultDialer));
                    }

                    if (packageName.equals(SYS_PKG) && useSystemDialer) {
                        resolveInfo.add(getSysResolveinfo());
                    }

                    if (packageName.equals(COMPANION_PKG)) {
                        resolveInfo.add(getCompanionResolveinfo());
                    }

                    if (packageName.equals(CAR_PKG)) {
                        resolveInfo.add(getCarModeResolveinfo(CAR_PKG, CAR_CLASS,
                                includeExternalCalls, includeSelfManagedCallsInCarModeDialer));
                    }

                    if (packageName.equals(CAR2_PKG)) {
                        resolveInfo.add(getCarModeResolveinfo(CAR2_PKG, CAR2_CLASS,
                                includeExternalCalls, includeSelfManagedCallsInCarModeDialer));
                    }
                } else {
                    // InCallController uses a blank package name when querying for non-ui incalls
                    if (useNonUiInCalls) {
                        resolveInfo.add(getNonUiResolveinfo());
                    }
                }

                return resolveInfo;
            }
        }).when(mMockPackageManager).queryIntentServicesAsUser(
                any(Intent.class), eq(PackageManager.GET_META_DATA), eq(CURRENT_USER_ID));
    }

    private void setupMockPackageManagerLocationPermission(final String pkg,
            final boolean granted) {
        when(mMockPackageManager.checkPermission(Manifest.permission.ACCESS_FINE_LOCATION, pkg))
                .thenReturn(granted
                        ? PackageManager.PERMISSION_GRANTED
                        : PackageManager.PERMISSION_DENIED);
  }
}
