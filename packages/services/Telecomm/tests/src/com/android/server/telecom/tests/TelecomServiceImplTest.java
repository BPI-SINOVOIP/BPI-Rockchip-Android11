/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static android.Manifest.permission.CALL_PHONE;
import static android.Manifest.permission.CALL_PRIVILEGED;
import static android.Manifest.permission.MODIFY_PHONE_STATE;
import static android.Manifest.permission.READ_PHONE_NUMBERS;
import static android.Manifest.permission.READ_PHONE_STATE;
import static android.Manifest.permission.READ_PRIVILEGED_PHONE_STATE;

import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.TelephonyManager;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.internal.telecom.ITelecomService;
import com.android.server.telecom.Call;
import com.android.server.telecom.CallIntentProcessor;
import com.android.server.telecom.CallState;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.DefaultDialerCache;
import com.android.server.telecom.PhoneAccountRegistrar;
import com.android.server.telecom.TelecomServiceImpl;
import com.android.server.telecom.TelecomSystem;
import com.android.server.telecom.components.UserCallIntentProcessor;
import com.android.server.telecom.components.UserCallIntentProcessorFactory;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.function.IntConsumer;

import static android.Manifest.permission.READ_SMS;
import static android.Manifest.permission.REGISTER_SIM_SUBSCRIPTION;
import static android.Manifest.permission.WRITE_SECURE_SETTINGS;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Matchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(JUnit4.class)
public class TelecomServiceImplTest extends TelecomTestCase {

    public static final String TEST_PACKAGE = "com.test";

    public static class CallIntentProcessAdapterFake implements CallIntentProcessor.Adapter {
        @Override
        public void processOutgoingCallIntent(Context context, CallsManager callsManager,
                Intent intent, String callingPackage) {

        }

        @Override
        public void processIncomingCallIntent(CallsManager callsManager, Intent intent) {

        }

        @Override
        public void processUnknownCallIntent(CallsManager callsManager, Intent intent) {

        }
    }

    public static class SubscriptionManagerAdapterFake
            implements TelecomServiceImpl.SubscriptionManagerAdapter {
        @Override
        public int getDefaultVoiceSubId() {
            return 0;
        }
    }

    public static class SettingsSecureAdapterFake implements
        TelecomServiceImpl.SettingsSecureAdapter {
        @Override
        public void putStringForUser(ContentResolver resolver, String name, String value,
            int userHandle) {

        }

        @Override
        public String getStringForUser(ContentResolver resolver, String name, int userHandle) {
            return THIRD_PARTY_CALL_SCREENING.flattenToString();
        }
    }

    private static class AnyStringIn implements ArgumentMatcher<String> {
        private Collection<String> mStrings;
        public AnyStringIn(Collection<String> strings) {
            this.mStrings = strings;
        }

        @Override
        public boolean matches(String string) {
            return mStrings.contains(string);
        }
    }

    private ITelecomService.Stub mTSIBinder;
    private AppOpsManager mAppOpsManager;
    private UserManager mUserManager;

    @Mock private CallsManager mFakeCallsManager;
    @Mock private PhoneAccountRegistrar mFakePhoneAccountRegistrar;
    @Mock private TelecomManager mTelecomManager;
    private CallIntentProcessor.Adapter mCallIntentProcessorAdapter =
            spy(new CallIntentProcessAdapterFake());
    @Mock private DefaultDialerCache mDefaultDialerCache;
    private IntConsumer mDefaultDialerObserver;
    private TelecomServiceImpl.SubscriptionManagerAdapter mSubscriptionManagerAdapter =
            spy(new SubscriptionManagerAdapterFake());
    private TelecomServiceImpl.SettingsSecureAdapter mSettingsSecureAdapter =
        spy(new SettingsSecureAdapterFake());
    @Mock private UserCallIntentProcessor mUserCallIntentProcessor;
    private PackageManager mPackageManager;
    @Mock private ApplicationInfo mApplicationInfo;

    private final TelecomSystem.SyncRoot mLock = new TelecomSystem.SyncRoot() { };

    private static final String DEFAULT_DIALER_PACKAGE = "com.google.android.dialer";
    private static final UserHandle USER_HANDLE_16 = new UserHandle(16);
    private static final UserHandle USER_HANDLE_17 = new UserHandle(17);
    private static final PhoneAccountHandle TEL_PA_HANDLE_16 = new PhoneAccountHandle(
            new ComponentName("test", "telComponentName"), "0", USER_HANDLE_16);
    private static final PhoneAccountHandle SIP_PA_HANDLE_17 = new PhoneAccountHandle(
            new ComponentName("test", "sipComponentName"), "1", USER_HANDLE_17);
    private static final PhoneAccountHandle TEL_PA_HANDLE_CURRENT = new PhoneAccountHandle(
            new ComponentName("test", "telComponentName"), "2", Binder.getCallingUserHandle());
    private static final PhoneAccountHandle SIP_PA_HANDLE_CURRENT = new PhoneAccountHandle(
            new ComponentName("test", "sipComponentName"), "3", Binder.getCallingUserHandle());
    private static final ComponentName THIRD_PARTY_CALL_SCREENING = new ComponentName("com.android" +
            ".thirdparty", "com.android.thirdparty.callscreeningserviceimpl");

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        mContext = mComponentContextFixture.getTestDouble().getApplicationContext();

        TelephonyManager mockTelephonyManager =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        when(mockTelephonyManager.isVoiceCapable()).thenReturn(true);

        doReturn(mContext).when(mContext).getApplicationContext();
        doNothing().when(mContext).sendBroadcastAsUser(any(Intent.class), any(UserHandle.class),
                anyString());
        doAnswer(invocation -> {
            mDefaultDialerObserver = invocation.getArgument(1);
            return null;
        }).when(mDefaultDialerCache).observeDefaultDialerApplication(any(Executor.class),
                any(IntConsumer.class));
        TelecomServiceImpl telecomServiceImpl = new TelecomServiceImpl(
                mContext,
                mFakeCallsManager,
                mFakePhoneAccountRegistrar,
                mCallIntentProcessorAdapter,
                new UserCallIntentProcessorFactory() {
                    @Override
                    public UserCallIntentProcessor create(Context context, UserHandle userHandle) {
                        return mUserCallIntentProcessor;
                    }
                },
                mDefaultDialerCache,
                mSubscriptionManagerAdapter,
                mSettingsSecureAdapter,
                mLock);
        mTSIBinder = telecomServiceImpl.getBinder();
        mComponentContextFixture.setTelecomManager(mTelecomManager);
        when(mTelecomManager.getDefaultDialerPackage()).thenReturn(DEFAULT_DIALER_PACKAGE);
        when(mTelecomManager.getSystemDialerPackage()).thenReturn(DEFAULT_DIALER_PACKAGE);

        mAppOpsManager = (AppOpsManager) mContext.getSystemService(Context.APP_OPS_SERVICE);
        mUserManager = (UserManager) mContext.getSystemService(Context.USER_SERVICE);

        when(mDefaultDialerCache.getDefaultDialerApplication(anyInt()))
                .thenReturn(DEFAULT_DIALER_PACKAGE);
        when(mDefaultDialerCache.isDefaultOrSystemDialer(eq(DEFAULT_DIALER_PACKAGE), anyInt()))
                .thenReturn(true);

        mPackageManager = mContext.getPackageManager();
    }

    @Override
    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @SmallTest
    @Test
    public void testGetDefaultOutgoingPhoneAccount() throws RemoteException {
        when(mFakePhoneAccountRegistrar
                .getOutgoingPhoneAccountForScheme(eq("tel"), any(UserHandle.class)))
                .thenReturn(TEL_PA_HANDLE_16);
        when(mFakePhoneAccountRegistrar
                .getOutgoingPhoneAccountForScheme(eq("sip"), any(UserHandle.class)))
                .thenReturn(SIP_PA_HANDLE_17);
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_16, SIP_PA_HANDLE_17);
        PhoneAccount phoneAccount = makePhoneAccount(TEL_PA_HANDLE_CURRENT).build();
        phoneAccount.setIsEnabled(true);
        doReturn(phoneAccount).when(mFakePhoneAccountRegistrar).getPhoneAccount(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        doNothing().when(mAppOpsManager).checkPackage(anyInt(), anyString());

        PhoneAccountHandle returnedHandleTel
                = mTSIBinder.getDefaultOutgoingPhoneAccount("tel", DEFAULT_DIALER_PACKAGE, null);
        assertEquals(TEL_PA_HANDLE_16, returnedHandleTel);

        PhoneAccountHandle returnedHandleSip
                = mTSIBinder.getDefaultOutgoingPhoneAccount("sip", DEFAULT_DIALER_PACKAGE, null);
        assertEquals(SIP_PA_HANDLE_17, returnedHandleSip);
    }

    @SmallTest
    @Test
    public void testGetDefaultOutgoingPhoneAccountSucceedsIfCallerIsSimCallManager()
            throws RemoteException {
        when(mFakePhoneAccountRegistrar
                .getOutgoingPhoneAccountForScheme(eq("tel"), any(UserHandle.class)))
                .thenReturn(TEL_PA_HANDLE_16);
        when(mFakePhoneAccountRegistrar
                .getOutgoingPhoneAccountForScheme(eq("sip"), any(UserHandle.class)))
                .thenReturn(SIP_PA_HANDLE_17);
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_16, SIP_PA_HANDLE_17);
        PhoneAccount phoneAccount = makePhoneAccount(TEL_PA_HANDLE_CURRENT).build();
        phoneAccount.setIsEnabled(true);
        doReturn(phoneAccount).when(mFakePhoneAccountRegistrar).getPhoneAccount(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        doReturn(TEL_PA_HANDLE_CURRENT).when(mFakePhoneAccountRegistrar)
                .getSimCallManagerFromHandle(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        // doNothing will make #isCallerSimCallManager return true
        doNothing().when(mAppOpsManager).checkPackage(anyInt(), anyString());
        doThrow(new SecurityException()).when(mContext)
                .enforceCallingOrSelfPermission(eq(READ_PRIVILEGED_PHONE_STATE), anyString());

        PhoneAccountHandle returnedHandleTel
                = mTSIBinder.getDefaultOutgoingPhoneAccount("tel", DEFAULT_DIALER_PACKAGE, null);
        assertEquals(TEL_PA_HANDLE_16, returnedHandleTel);

        PhoneAccountHandle returnedHandleSip
                = mTSIBinder.getDefaultOutgoingPhoneAccount("sip", DEFAULT_DIALER_PACKAGE, null);
        assertEquals(SIP_PA_HANDLE_17, returnedHandleSip);
    }

    @SmallTest
    @Test
    public void testGetDefaultOutgoingPhoneAccountFailure() throws RemoteException {
        // make sure that the list of user profiles doesn't include anything the PhoneAccountHandles
        // are associated with

        when(mFakePhoneAccountRegistrar
                .getOutgoingPhoneAccountForScheme(eq("tel"), any(UserHandle.class)))
                .thenReturn(TEL_PA_HANDLE_16);
        when(mFakePhoneAccountRegistrar.getPhoneAccountUnchecked(TEL_PA_HANDLE_16)).thenReturn(
                makePhoneAccount(TEL_PA_HANDLE_16).build());
        when(mAppOpsManager.noteOp(eq(AppOpsManager.OP_READ_PHONE_STATE), anyInt(), anyString(),
                nullable(String.class), nullable(String.class)))
                .thenReturn(AppOpsManager.MODE_IGNORED);
        PhoneAccount phoneAccount = makePhoneAccount(TEL_PA_HANDLE_CURRENT).build();
        phoneAccount.setIsEnabled(true);
        doReturn(phoneAccount).when(mFakePhoneAccountRegistrar).getPhoneAccount(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        doReturn(TEL_PA_HANDLE_16).when(mFakePhoneAccountRegistrar).getSimCallManagerFromHandle(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        doNothing().when(mAppOpsManager).checkPackage(anyInt(), anyString());
        doThrow(new SecurityException()).when(mContext)
                .enforceCallingOrSelfPermission(eq(READ_PRIVILEGED_PHONE_STATE), anyString());

        PhoneAccountHandle returnedHandleTel
                = mTSIBinder.getDefaultOutgoingPhoneAccount("tel", "", null);
        assertNull(returnedHandleTel);
    }

    @SmallTest
    @Test
    public void testGetUserSelectedOutgoingPhoneAccount() throws RemoteException {
        when(mFakePhoneAccountRegistrar.getUserSelectedOutgoingPhoneAccount(any(UserHandle.class)))
                .thenReturn(TEL_PA_HANDLE_16);
        when(mFakePhoneAccountRegistrar.getPhoneAccountUnchecked(TEL_PA_HANDLE_16)).thenReturn(
                makeMultiUserPhoneAccount(TEL_PA_HANDLE_16).build());

        PhoneAccountHandle returnedHandle
                = mTSIBinder.getUserSelectedOutgoingPhoneAccount(
                        TEL_PA_HANDLE_16.getComponentName().getPackageName());
        assertEquals(TEL_PA_HANDLE_16, returnedHandle);
    }

    @SmallTest
    @Test
    public void testSetUserSelectedOutgoingPhoneAccount() throws RemoteException {
        mTSIBinder.setUserSelectedOutgoingPhoneAccount(TEL_PA_HANDLE_16);
        verify(mFakePhoneAccountRegistrar)
                .setUserSelectedOutgoingPhoneAccount(eq(TEL_PA_HANDLE_16), any(UserHandle.class));
    }

    @SmallTest
    @Test
    public void testSetUserSelectedOutgoingPhoneAccountFailure() throws RemoteException {
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(
                anyString(), nullable(String.class));
        try {
            mTSIBinder.setUserSelectedOutgoingPhoneAccount(TEL_PA_HANDLE_16);
        } catch (SecurityException e) {
            // desired result
        }
        verify(mFakePhoneAccountRegistrar, never())
                .setUserSelectedOutgoingPhoneAccount(
                        any(PhoneAccountHandle.class), any(UserHandle.class));
    }

    @SmallTest
    @Test
    public void testGetCallCapablePhoneAccounts() throws RemoteException {
        List<PhoneAccountHandle> fullPHList = new ArrayList<PhoneAccountHandle>() {{
            add(TEL_PA_HANDLE_16);
            add(SIP_PA_HANDLE_17);
        }};

        List<PhoneAccountHandle> smallPHList = new ArrayList<PhoneAccountHandle>() {{
            add(SIP_PA_HANDLE_17);
        }};
        // Returns all phone accounts when getCallCapablePhoneAccounts is called.
        when(mFakePhoneAccountRegistrar
                .getCallCapablePhoneAccounts(nullable(String.class), eq(true),
                        nullable(UserHandle.class))).thenReturn(fullPHList);
        // Returns only enabled phone accounts when getCallCapablePhoneAccounts is called.
        when(mFakePhoneAccountRegistrar
                .getCallCapablePhoneAccounts(nullable(String.class), eq(false),
                        nullable(UserHandle.class))).thenReturn(smallPHList);
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_16, SIP_PA_HANDLE_17);

        assertEquals(fullPHList,
                mTSIBinder.getCallCapablePhoneAccounts(true, DEFAULT_DIALER_PACKAGE, null));
        assertEquals(smallPHList,
                mTSIBinder.getCallCapablePhoneAccounts(false, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetCallCapablePhoneAccountsFailure() throws RemoteException {
        List<String> enforcedPermissions = new ArrayList<String>() {{
            add(READ_PHONE_STATE);
            add(READ_PRIVILEGED_PHONE_STATE);
        }};
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(
                argThat(new AnyStringIn(enforcedPermissions)), anyString());

        List<PhoneAccountHandle> result = null;
        try {
            result = mTSIBinder.getCallCapablePhoneAccounts(true, "", null);
        } catch (SecurityException e) {
            // intended behavior
        }
        assertNull(result);
        verify(mFakePhoneAccountRegistrar, never())
                .getCallCapablePhoneAccounts(anyString(), anyBoolean(), any(UserHandle.class));
    }

    @SmallTest
    @Test
    public void testGetPhoneAccountsSupportingScheme() throws RemoteException {
        List<PhoneAccountHandle> sipPHList = new ArrayList<PhoneAccountHandle>() {{
            add(SIP_PA_HANDLE_17);
        }};

        List<PhoneAccountHandle> telPHList = new ArrayList<PhoneAccountHandle>() {{
            add(TEL_PA_HANDLE_16);
        }};
        when(mFakePhoneAccountRegistrar
                .getCallCapablePhoneAccounts(eq("tel"), anyBoolean(), any(UserHandle.class)))
                .thenReturn(telPHList);
        when(mFakePhoneAccountRegistrar
                .getCallCapablePhoneAccounts(eq("sip"), anyBoolean(), any(UserHandle.class)))
                .thenReturn(sipPHList);
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_16, SIP_PA_HANDLE_17);

        assertEquals(telPHList,
                mTSIBinder.getPhoneAccountsSupportingScheme("tel", DEFAULT_DIALER_PACKAGE));
        assertEquals(sipPHList,
                mTSIBinder.getPhoneAccountsSupportingScheme("sip", DEFAULT_DIALER_PACKAGE));
    }

    @SmallTest
    @Test
    public void testGetPhoneAccountsForPackage() throws RemoteException {
        List<PhoneAccountHandle> phoneAccountHandleList = new ArrayList<PhoneAccountHandle>() {{
            add(TEL_PA_HANDLE_16);
            add(SIP_PA_HANDLE_17);
        }};
        when(mFakePhoneAccountRegistrar
                .getPhoneAccountsForPackage(anyString(), any(UserHandle.class)))
                .thenReturn(phoneAccountHandleList);
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_16, SIP_PA_HANDLE_17);
        assertEquals(phoneAccountHandleList,
                mTSIBinder.getPhoneAccountsForPackage(
                        TEL_PA_HANDLE_16.getComponentName().getPackageName()));
    }

    @SmallTest
    @Test
    public void testGetPhoneAccount() throws RemoteException {
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_16, SIP_PA_HANDLE_17);
        assertEquals(TEL_PA_HANDLE_16, mTSIBinder.getPhoneAccount(TEL_PA_HANDLE_16)
                .getAccountHandle());
        assertEquals(SIP_PA_HANDLE_17, mTSIBinder.getPhoneAccount(SIP_PA_HANDLE_17)
                .getAccountHandle());
    }

    @SmallTest
    @Test
    public void testGetAllPhoneAccounts() throws RemoteException {
        List<PhoneAccount> phoneAccountList = new ArrayList<PhoneAccount>() {{
            add(makePhoneAccount(TEL_PA_HANDLE_16).build());
            add(makePhoneAccount(SIP_PA_HANDLE_17).build());
        }};
        when(mFakePhoneAccountRegistrar.getAllPhoneAccounts(any(UserHandle.class)))
                .thenReturn(phoneAccountList);

        assertEquals(2, mTSIBinder.getAllPhoneAccounts().size());
    }

    @SmallTest
    @Test
    public void testRegisterPhoneAccount() throws RemoteException {
        String packageNameToUse = "com.android.officialpackage";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "test", Binder.getCallingUserHandle());
        PhoneAccount phoneAccount = makePhoneAccount(phHandle).build();
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);

        registerPhoneAccountTestHelper(phoneAccount, true);
    }

    @SmallTest
    @Test
    public void testRegisterPhoneAccountWithoutModifyPermission() throws RemoteException {
        // tests the case where the package does not have MODIFY_PHONE_STATE but is
        // registering its own phone account as a third-party connection service
        String packageNameToUse = "com.thirdparty.connectionservice";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "asdf", Binder.getCallingUserHandle());
        PhoneAccount phoneAccount = makePhoneAccount(phHandle).build();

        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);
        PackageManager pm = mContext.getPackageManager();
        when(pm.hasSystemFeature(PackageManager.FEATURE_CONNECTION_SERVICE)).thenReturn(true);

        registerPhoneAccountTestHelper(phoneAccount, true);
    }

    @SmallTest
    @Test
    public void testRegisterPhoneAccountWithoutModifyPermissionFailure() throws RemoteException {
        // tests the case where the third party package should not be allowed to register a phone
        // account due to the lack of modify permission.
        String packageNameToUse = "com.thirdparty.connectionservice";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "asdf", Binder.getCallingUserHandle());
        PhoneAccount phoneAccount = makePhoneAccount(phHandle).build();

        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);
        PackageManager pm = mContext.getPackageManager();
        when(pm.hasSystemFeature(PackageManager.FEATURE_CONNECTION_SERVICE)).thenReturn(false);

        registerPhoneAccountTestHelper(phoneAccount, false);
    }

    @SmallTest
    @Test
    public void testRegisterPhoneAccountWithoutSimSubscriptionPermissionFailure()
            throws RemoteException {
        String packageNameToUse = "com.thirdparty.connectionservice";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "asdf", Binder.getCallingUserHandle());
        PhoneAccount phoneAccount = makePhoneAccount(phHandle)
                .setCapabilities(PhoneAccount.CAPABILITY_SIM_SUBSCRIPTION).build();

        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);
        doThrow(new SecurityException())
                .when(mContext)
                .enforceCallingOrSelfPermission(eq(REGISTER_SIM_SUBSCRIPTION),
                        nullable(String.class));

        registerPhoneAccountTestHelper(phoneAccount, false);
    }

    @SmallTest
    @Test
    public void testRegisterPhoneAccountWithoutMultiUserPermissionFailure()
            throws Exception {
        String packageNameToUse = "com.thirdparty.connectionservice";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "asdf", Binder.getCallingUserHandle());
        PhoneAccount phoneAccount = makeMultiUserPhoneAccount(phHandle).build();

        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);

        PackageManager packageManager = mContext.getPackageManager();
        when(packageManager.getApplicationInfo(packageNameToUse, PackageManager.GET_META_DATA))
                .thenReturn(new ApplicationInfo());

        registerPhoneAccountTestHelper(phoneAccount, false);
    }

    private void registerPhoneAccountTestHelper(PhoneAccount testPhoneAccount,
            boolean shouldSucceed) throws RemoteException {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        boolean didExceptionOccur = false;
        try {
            mTSIBinder.registerPhoneAccount(testPhoneAccount);
        } catch (Exception e) {
            didExceptionOccur = true;
        }

        if (shouldSucceed) {
            assertFalse(didExceptionOccur);
            verify(mFakePhoneAccountRegistrar).registerPhoneAccount(testPhoneAccount);
        } else {
            assertTrue(didExceptionOccur);
            verify(mFakePhoneAccountRegistrar, never())
                    .registerPhoneAccount(any(PhoneAccount.class));
        }
    }

    @SmallTest
    @Test
    public void testUnregisterPhoneAccount() throws RemoteException {
        String packageNameToUse = "com.android.officialpackage";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "test", Binder.getCallingUserHandle());

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);

        mTSIBinder.unregisterPhoneAccount(phHandle);
        verify(mFakePhoneAccountRegistrar).unregisterPhoneAccount(phHandle);
    }

    @SmallTest
    @Test
    public void testUnregisterPhoneAccountFailure() throws RemoteException {
        String packageNameToUse = "com.thirdparty.connectionservice";
        PhoneAccountHandle phHandle = new PhoneAccountHandle(new ComponentName(
                packageNameToUse, "cs"), "asdf", Binder.getCallingUserHandle());

        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingOrSelfPermission(MODIFY_PHONE_STATE);
        PackageManager pm = mContext.getPackageManager();
        when(pm.hasSystemFeature(PackageManager.FEATURE_CONNECTION_SERVICE)).thenReturn(false);

        try {
            mTSIBinder.unregisterPhoneAccount(phHandle);
        } catch (UnsupportedOperationException e) {
            // expected behavior
        }
        verify(mFakePhoneAccountRegistrar, never())
                .unregisterPhoneAccount(any(PhoneAccountHandle.class));
        verify(mContext, never())
                .sendBroadcastAsUser(any(Intent.class), any(UserHandle.class), anyString());
    }

    @SmallTest
    @Test
    public void testAddNewIncomingCall() throws Exception {
        PhoneAccount phoneAccount = makePhoneAccount(TEL_PA_HANDLE_CURRENT).build();
        phoneAccount.setIsEnabled(true);
        doReturn(phoneAccount).when(mFakePhoneAccountRegistrar).getPhoneAccount(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        doNothing().when(mAppOpsManager).checkPackage(anyInt(), anyString());
        Bundle extras = createSampleExtras();

        mTSIBinder.addNewIncomingCall(TEL_PA_HANDLE_CURRENT, extras);

        addCallTestHelper(TelecomManager.ACTION_INCOMING_CALL,
                CallIntentProcessor.KEY_IS_INCOMING_CALL, extras, false);
    }

    @SmallTest
    @Test
    public void testAddNewIncomingCallFailure() throws Exception {
        try {
            mTSIBinder.addNewIncomingCall(TEL_PA_HANDLE_16, null);
        } catch (SecurityException e) {
            // expected
        }

        doThrow(new SecurityException()).when(mAppOpsManager).checkPackage(anyInt(), anyString());

        try {
            mTSIBinder.addNewIncomingCall(TEL_PA_HANDLE_CURRENT, null);
        } catch (SecurityException e) {
            // expected
        }

        // Verify that neither of these attempts got through
        verify(mCallIntentProcessorAdapter, never())
                .processIncomingCallIntent(any(CallsManager.class), any(Intent.class));
    }

    @SmallTest
    @Test
    public void testAddNewUnknownCall() throws Exception {
        PhoneAccount phoneAccount = makePhoneAccount(TEL_PA_HANDLE_CURRENT).build();
        phoneAccount.setIsEnabled(true);
        doReturn(phoneAccount).when(mFakePhoneAccountRegistrar).getPhoneAccount(
                eq(TEL_PA_HANDLE_CURRENT), any(UserHandle.class));
        doNothing().when(mAppOpsManager).checkPackage(anyInt(), anyString());
        Bundle extras = createSampleExtras();

        mTSIBinder.addNewUnknownCall(TEL_PA_HANDLE_CURRENT, extras);

        addCallTestHelper(TelecomManager.ACTION_NEW_UNKNOWN_CALL,
                CallIntentProcessor.KEY_IS_UNKNOWN_CALL, extras, true);
    }

    @SmallTest
    @Test
    public void testAddNewUnknownCallFailure() throws Exception {
        try {
            mTSIBinder.addNewUnknownCall(TEL_PA_HANDLE_16, null);
        } catch (SecurityException e) {
            // expected
        }

        doThrow(new SecurityException()).when(mAppOpsManager).checkPackage(anyInt(), anyString());

        try {
            mTSIBinder.addNewUnknownCall(TEL_PA_HANDLE_CURRENT, null);
        } catch (SecurityException e) {
            // expected
        }

        // Verify that neither of these attempts got through
        verify(mCallIntentProcessorAdapter, never())
                .processIncomingCallIntent(any(CallsManager.class), any(Intent.class));
    }

    private void addCallTestHelper(String expectedAction, String extraCallKey,
            Bundle expectedExtras, boolean isUnknown) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        if (isUnknown) {
            verify(mCallIntentProcessorAdapter).processUnknownCallIntent(any(CallsManager.class),
                    intentCaptor.capture());
        } else {
            verify(mCallIntentProcessorAdapter).processIncomingCallIntent(any(CallsManager.class),
                    intentCaptor.capture());
        }
        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(expectedAction, capturedIntent.getAction());
        Bundle intentExtras = capturedIntent.getExtras();
        assertEquals(TEL_PA_HANDLE_CURRENT,
                intentExtras.get(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE));
        assertTrue(intentExtras.getBoolean(extraCallKey));

        if (isUnknown) {
            for (String expectedKey : expectedExtras.keySet()) {
                assertTrue(intentExtras.containsKey(expectedKey));
                assertEquals(expectedExtras.get(expectedKey), intentExtras.get(expectedKey));
            }
        }
        else {
            assertTrue(areBundlesEqual(expectedExtras,
                    (Bundle) intentExtras.get(TelecomManager.EXTRA_INCOMING_CALL_EXTRAS)));
        }
    }

    @SmallTest
    @Test
    public void testPlaceCallWithNonEmergencyPermission() throws Exception {
        Uri handle = Uri.parse("tel:6505551234");
        Bundle extras = createSampleExtras();

        when(mAppOpsManager.noteOp(eq(AppOpsManager.OP_CALL_PHONE), anyInt(), anyString(),
                nullable(String.class), nullable(String.class)))
                .thenReturn(AppOpsManager.MODE_ALLOWED);
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext).checkCallingPermission(CALL_PHONE);
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingPermission(CALL_PRIVILEGED);

        mTSIBinder.placeCall(handle, extras, DEFAULT_DIALER_PACKAGE, null);
        placeCallTestHelper(handle, extras, true);
    }

    @SmallTest
    @Test
    public void testPlaceCallWithAppOpsOff() throws Exception {
        Uri handle = Uri.parse("tel:6505551234");
        Bundle extras = createSampleExtras();

        when(mAppOpsManager.noteOp(eq(AppOpsManager.OP_CALL_PHONE), anyInt(), anyString(),
                nullable(String.class), nullable(String.class)))
                .thenReturn(AppOpsManager.MODE_IGNORED);
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext).checkCallingPermission(CALL_PHONE);
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingPermission(CALL_PRIVILEGED);

        mTSIBinder.placeCall(handle, extras, DEFAULT_DIALER_PACKAGE, null);
        placeCallTestHelper(handle, extras, false);
    }

    @SmallTest
    @Test
    public void testPlaceCallWithNoCallingPermission() throws Exception {
        Uri handle = Uri.parse("tel:6505551234");
        Bundle extras = createSampleExtras();

        when(mAppOpsManager.noteOp(eq(AppOpsManager.OP_CALL_PHONE), anyInt(), anyString(),
                nullable(String.class), nullable(String.class)))
                .thenReturn(AppOpsManager.MODE_ALLOWED);
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingPermission(CALL_PHONE);
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext).checkCallingPermission(CALL_PRIVILEGED);

        mTSIBinder.placeCall(handle, extras, DEFAULT_DIALER_PACKAGE, null);
        placeCallTestHelper(handle, extras, false);
    }

    private void placeCallTestHelper(Uri expectedHandle, Bundle expectedExtras,
            boolean shouldNonEmergencyBeAllowed) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mUserCallIntentProcessor).processIntent(intentCaptor.capture(), anyString(),
                eq(shouldNonEmergencyBeAllowed), eq(true));
        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_CALL, capturedIntent.getAction());
        assertEquals(expectedHandle, capturedIntent.getData());
        assertTrue(areBundlesEqual(expectedExtras, capturedIntent.getExtras()));
    }

    @SmallTest
    @Test
    public void testPlaceCallFailure() throws Exception {
        Uri handle = Uri.parse("tel:6505551234");
        Bundle extras = createSampleExtras();

        doThrow(new SecurityException())
                .when(mContext).enforceCallingOrSelfPermission(eq(CALL_PHONE), anyString());

        try {
            mTSIBinder.placeCall(handle, extras, "arbitrary_package_name", null);
        } catch (SecurityException e) {
            // expected
        }

        verify(mUserCallIntentProcessor, never())
                .processIntent(any(Intent.class), anyString(), anyBoolean(), eq(true));
    }

    @SmallTest
    @Test
    public void testSetDefaultDialer() throws Exception {
        String packageName = "sample.package";
        int currentUser = ActivityManager.getCurrentUser();

        String[] defaultDialer = new String[1];
        doAnswer(invocation -> {
            defaultDialer[0] = packageName;
            mDefaultDialerObserver.accept(currentUser);
            return true;
        }).when(mDefaultDialerCache).setDefaultDialer(eq(packageName), eq(currentUser));
        doAnswer(invocation -> defaultDialer[0]).when(mDefaultDialerCache)
                .getDefaultDialerApplication(eq(currentUser));

        mTSIBinder.setDefaultDialer(packageName);

        verify(mDefaultDialerCache).setDefaultDialer(eq(packageName), eq(currentUser));
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).sendBroadcastAsUser(intentCaptor.capture(), any(UserHandle.class));
        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(TelecomManager.ACTION_DEFAULT_DIALER_CHANGED, capturedIntent.getAction());
        String packageNameExtra = capturedIntent.getStringExtra(
                TelecomManager.EXTRA_CHANGE_DEFAULT_DIALER_PACKAGE_NAME);
        assertEquals(packageName, packageNameExtra);
    }

    @SmallTest
    @Test
    public void testSetDefaultDialerNoModifyPhoneStatePermission() throws Exception {
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(
                eq(MODIFY_PHONE_STATE), nullable(String.class));
        setDefaultDialerFailureTestHelper();
    }

    @SmallTest
    @Test
    public void testSetDefaultDialerNoWriteSecureSettingsPermission() throws Exception {
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(
                eq(WRITE_SECURE_SETTINGS), nullable(String.class));
        setDefaultDialerFailureTestHelper();
    }

    private void setDefaultDialerFailureTestHelper() throws Exception {
        boolean exceptionThrown = false;
        try {
            mTSIBinder.setDefaultDialer(DEFAULT_DIALER_PACKAGE);
        } catch (SecurityException e) {
            exceptionThrown = true;
        }
        assertTrue(exceptionThrown);
        verify(mDefaultDialerCache, never()).setDefaultDialer(anyString(), anyInt());
        verify(mContext, never()).sendBroadcastAsUser(any(Intent.class), any(UserHandle.class));
    }

    @SmallTest
    @Test
    public void testIsVoicemailNumber() throws Exception {
        String vmNumber = "010";
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_CURRENT);

        doReturn(true).when(mFakePhoneAccountRegistrar).isVoiceMailNumber(TEL_PA_HANDLE_CURRENT,
                vmNumber);
        assertTrue(mTSIBinder.isVoiceMailNumber(TEL_PA_HANDLE_CURRENT,
                vmNumber, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testIsVoicemailNumberAccountNotVisibleFailure() throws Exception {
        String vmNumber = "010";

        doReturn(true).when(mFakePhoneAccountRegistrar).isVoiceMailNumber(TEL_PA_HANDLE_CURRENT,
                vmNumber);

        when(mFakePhoneAccountRegistrar.getPhoneAccount(TEL_PA_HANDLE_CURRENT,
                Binder.getCallingUserHandle())).thenReturn(null);
        assertFalse(mTSIBinder
                .isVoiceMailNumber(TEL_PA_HANDLE_CURRENT, vmNumber, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetVoicemailNumberWithNullAccountHandle() throws Exception {
        when(mFakePhoneAccountRegistrar.getPhoneAccount(isNull(PhoneAccountHandle.class),
                eq(Binder.getCallingUserHandle())))
                .thenReturn(makePhoneAccount(TEL_PA_HANDLE_CURRENT).build());
        int subId = 58374;
        String vmNumber = "543";
        doReturn(subId).when(mSubscriptionManagerAdapter).getDefaultVoiceSubId();

        TelephonyManager mockTelephonyManager =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        when(mockTelephonyManager.getVoiceMailNumber()).thenReturn(vmNumber);

        assertEquals(vmNumber, mTSIBinder.getVoiceMailNumber(null, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetVoicemailNumberWithNonNullAccountHandle() throws Exception {
        when(mFakePhoneAccountRegistrar.getPhoneAccount(eq(TEL_PA_HANDLE_CURRENT),
                eq(Binder.getCallingUserHandle())))
                .thenReturn(makePhoneAccount(TEL_PA_HANDLE_CURRENT).build());
        int subId = 58374;
        String vmNumber = "543";

        TelephonyManager mockTelephonyManager =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        when(mockTelephonyManager.getVoiceMailNumber()).thenReturn(vmNumber);
        when(mFakePhoneAccountRegistrar.getSubscriptionIdForPhoneAccount(TEL_PA_HANDLE_CURRENT))
                .thenReturn(subId);

        assertEquals(vmNumber,
                mTSIBinder.getVoiceMailNumber(TEL_PA_HANDLE_CURRENT, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithNoPermissionTargetPreR() throws Exception {
        setupGetLine1NumberTest();
        setTargetSdkVersion(Build.VERSION_CODES.Q);

        try {
            String line1Number = mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT,
                    DEFAULT_DIALER_PACKAGE, null);
            fail("Should have thrown a SecurityException when invoking getLine1Number without "
                    + "permission, received "
                    + line1Number);
        } catch (SecurityException expected) {
        }
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithNoPermissionTargetR() throws Exception {
        setupGetLine1NumberTest();

        try {
            String line1Number = mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT,
                    DEFAULT_DIALER_PACKAGE, null);
            fail("Should have thrown a SecurityException when invoking getLine1Number without "
                    + "permission, received "
                    + line1Number);
        } catch (SecurityException expected) {
        }
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithReadPhoneStateTargetPreR() throws Exception {
        String line1Number = setupGetLine1NumberTest();
        setTargetSdkVersion(Build.VERSION_CODES.Q);
        grantPermissionAndAppOp(READ_PHONE_STATE, AppOpsManager.OPSTR_READ_PHONE_STATE);

        assertEquals(line1Number,
                mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithReadPhoneStateTargetR() throws Exception {
        setupGetLine1NumberTest();
        grantPermissionAndAppOp(READ_PHONE_STATE, AppOpsManager.OPSTR_READ_PHONE_STATE);

        try {
            String line1Number = mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT,
                    DEFAULT_DIALER_PACKAGE, null);
            fail("Should have thrown a SecurityException when invoking getLine1Number on target R"
                    + " with READ_PHONE_STATE permission, received "
                    + line1Number);
        } catch (SecurityException expected) {
        }
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithReadPhoneNumbersTargetR() throws Exception {
        String line1Number = setupGetLine1NumberTest();
        grantPermissionAndAppOp(READ_PHONE_NUMBERS, AppOpsManager.OPSTR_READ_PHONE_NUMBERS);

        assertEquals(line1Number,
                mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithReadSmsTargetR() throws Exception {
        String line1Number = setupGetLine1NumberTest();
        grantPermissionAndAppOp(READ_SMS, AppOpsManager.OPSTR_READ_SMS);

        assertEquals(line1Number,
                mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT, DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testGetLine1NumberWithWriteSmsTargetR() throws Exception {
        String line1Number = setupGetLine1NumberTest();
        doReturn(AppOpsManager.MODE_ALLOWED).when(mAppOpsManager).noteOpNoThrow(
                eq(AppOpsManager.OPSTR_WRITE_SMS), anyInt(), eq(DEFAULT_DIALER_PACKAGE), any(),
                any());

        assertEquals(line1Number,
                mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT, DEFAULT_DIALER_PACKAGE, null));
    }


    @SmallTest
    @Test
    public void testGetLine1NumberAsDefaultDialer() throws Exception {
        String line1Number = setupGetLine1NumberTest();
        doReturn(true).when(mDefaultDialerCache).isDefaultOrSystemDialer(
                eq(DEFAULT_DIALER_PACKAGE), anyInt());

        assertEquals(line1Number,
                mTSIBinder.getLine1Number(TEL_PA_HANDLE_CURRENT, DEFAULT_DIALER_PACKAGE, null));
    }

    private String setupGetLine1NumberTest() throws Exception {
        int subId = 58374;
        String line1Number = "9482752023479";

        setTargetSdkVersion(Build.VERSION_CODES.R);
        doReturn(AppOpsManager.MODE_DEFAULT).when(mAppOpsManager).noteOpNoThrow(anyString(),
                anyInt(), eq(DEFAULT_DIALER_PACKAGE), any(), any());
        makeAccountsVisibleToAllUsers(TEL_PA_HANDLE_CURRENT);
        when(mFakePhoneAccountRegistrar.getSubscriptionIdForPhoneAccount(TEL_PA_HANDLE_CURRENT))
                .thenReturn(subId);
        TelephonyManager mockTelephonyManager =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        when(mockTelephonyManager.getLine1Number()).thenReturn(line1Number);
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(anyString(),
                anyString());
        doReturn(PackageManager.PERMISSION_DENIED).when(mContext).checkCallingOrSelfPermission(
                anyString());
        doReturn(false).when(mDefaultDialerCache).isDefaultOrSystemDialer(
                eq(DEFAULT_DIALER_PACKAGE), anyInt());
        return line1Number;
    }

    private void grantPermissionAndAppOp(String permission, String appop) {
        doReturn(PackageManager.PERMISSION_GRANTED).when(mContext).checkCallingOrSelfPermission(
                eq(permission));
        doNothing().when(mContext).enforceCallingOrSelfPermission(eq(permission), anyString());
        doReturn(AppOpsManager.MODE_ALLOWED).when(mAppOpsManager).noteOp(eq(appop), anyInt(),
                eq(DEFAULT_DIALER_PACKAGE), any(), any());
        doReturn(AppOpsManager.MODE_ALLOWED).when(mAppOpsManager).noteOpNoThrow(eq(appop), anyInt(),
                eq(DEFAULT_DIALER_PACKAGE), any(), any());
    }

    private void setTargetSdkVersion(int targetSdkVersion) throws Exception {
        mApplicationInfo.targetSdkVersion = targetSdkVersion;
        doReturn(mApplicationInfo).when(mPackageManager).getApplicationInfoAsUser(
                eq(DEFAULT_DIALER_PACKAGE), anyInt(), any());
    }

    @SmallTest
    @Test
    public void testEndCallWithRingingForegroundCall() throws Exception {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.RINGING);
        when(mFakeCallsManager.getForegroundCall()).thenReturn(call);
        assertTrue(mTSIBinder.endCall(TEST_PACKAGE));
        verify(mFakeCallsManager).rejectCall(eq(call), eq(false), isNull());
    }

    @SmallTest
    @Test
    public void testEndCallWithSimulatedRingingForegroundCall() throws Exception {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.SIMULATED_RINGING);
        when(mFakeCallsManager.getForegroundCall()).thenReturn(call);
        assertTrue(mTSIBinder.endCall(TEST_PACKAGE));
        verify(mFakeCallsManager).rejectCall(eq(call), eq(false), isNull());
    }

    @SmallTest
    @Test
    public void testEndCallWithNonRingingForegroundCall() throws Exception {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.ACTIVE);
        when(mFakeCallsManager.getForegroundCall()).thenReturn(call);
        assertTrue(mTSIBinder.endCall(TEST_PACKAGE));
        verify(mFakeCallsManager).disconnectCall(eq(call));
    }

    @SmallTest
    @Test
    public void testEndCallWithNoForegroundCall() throws Exception {
        Call call = mock(Call.class);
        when(call.getState()).thenReturn(CallState.ACTIVE);
        when(mFakeCallsManager.getFirstCallWithState(any()))
                .thenReturn(call);
        assertTrue(mTSIBinder.endCall(TEST_PACKAGE));
        verify(mFakeCallsManager).disconnectCall(eq(call));
    }

    @SmallTest
    @Test
    public void testEndCallWithNoCalls() throws Exception {
        assertFalse(mTSIBinder.endCall(null));
    }

    @SmallTest
    @Test
    public void testAcceptRingingCall() throws Exception {
        Call call = mock(Call.class);
        when(mFakeCallsManager.getFirstCallWithState(anyInt(), anyInt())).thenReturn(call);
        // Not intended to be a real video state. Here to ensure that the call will be answered
        // with whatever video state it's currently in.
        int fakeVideoState = 29578215;
        when(call.getVideoState()).thenReturn(fakeVideoState);
        mTSIBinder.acceptRingingCall("");
        verify(mFakeCallsManager).answerCall(eq(call), eq(fakeVideoState));
    }

    @SmallTest
    @Test
    public void testAcceptRingingCallWithValidVideoState() throws Exception {
        Call call = mock(Call.class);
        when(mFakeCallsManager.getFirstCallWithState(anyInt(), anyInt())).thenReturn(call);
        // Not intended to be a real video state. Here to ensure that the call will be answered
        // with the video state passed in to acceptRingingCallWithVideoState
        int fakeVideoState = 29578215;
        int realVideoState = VideoProfile.STATE_RX_ENABLED | VideoProfile.STATE_TX_ENABLED;
        when(call.getVideoState()).thenReturn(fakeVideoState);
        mTSIBinder.acceptRingingCallWithVideoState("", realVideoState);
        verify(mFakeCallsManager).answerCall(eq(call), eq(realVideoState));
    }

    @SmallTest
    @Test
    public void testIsInCall() throws Exception {
        when(mFakeCallsManager.hasOngoingCalls()).thenReturn(true);
        assertTrue(mTSIBinder.isInCall(DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testNotIsInCall() throws Exception {
        when(mFakeCallsManager.hasOngoingCalls()).thenReturn(false);
        assertFalse(mTSIBinder.isInCall(DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testIsInCallFail() throws Exception {
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(
                anyString(), any());
        try {
            mTSIBinder.isInCall("blah", null);
            fail();
        } catch (SecurityException e) {
            // desired result
        }
        verify(mFakeCallsManager, never()).hasOngoingCalls();
    }

    @SmallTest
    @Test
    public void testIsInManagedCall() throws Exception {
        when(mFakeCallsManager.hasOngoingManagedCalls()).thenReturn(true);
        assertTrue(mTSIBinder.isInManagedCall(DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testNotIsInManagedCall() throws Exception {
        when(mFakeCallsManager.hasOngoingManagedCalls()).thenReturn(false);
        assertFalse(mTSIBinder.isInManagedCall(DEFAULT_DIALER_PACKAGE, null));
    }

    @SmallTest
    @Test
    public void testIsInManagedCallFail() throws Exception {
        doThrow(new SecurityException()).when(mContext).enforceCallingOrSelfPermission(
                anyString(), any());
        try {
            mTSIBinder.isInManagedCall("blah", null);
            fail();
        } catch (SecurityException e) {
            // desired result
        }
        verify(mFakeCallsManager, never()).hasOngoingCalls();
    }

    /**
     * Register phone accounts for the supplied PhoneAccountHandles to make them
     * visible to all users (via the isVisibleToCaller method in TelecomServiceImpl.
     * @param handles the handles for which phone accounts should be created for.
     */
    private void makeAccountsVisibleToAllUsers(PhoneAccountHandle... handles) {
        for (PhoneAccountHandle ph : handles) {
            when(mFakePhoneAccountRegistrar.getPhoneAccountUnchecked(eq(ph))).thenReturn(
                    makeMultiUserPhoneAccount(ph).build());
            when(mFakePhoneAccountRegistrar
                    .getPhoneAccount(eq(ph), nullable(UserHandle.class), anyBoolean()))
                    .thenReturn(makeMultiUserPhoneAccount(ph).build());
            when(mFakePhoneAccountRegistrar
                    .getPhoneAccount(eq(ph), nullable(UserHandle.class)))
                    .thenReturn(makeMultiUserPhoneAccount(ph).build());
        }
    }

    private PhoneAccount.Builder makeMultiUserPhoneAccount(PhoneAccountHandle paHandle) {
        PhoneAccount.Builder paBuilder = makePhoneAccount(paHandle);
        paBuilder.setCapabilities(PhoneAccount.CAPABILITY_MULTI_USER);
        return paBuilder;
    }

    private PhoneAccount.Builder makePhoneAccount(PhoneAccountHandle paHandle) {
        return new PhoneAccount.Builder(paHandle, "testLabel");
    }

    private Bundle createSampleExtras() {
        Bundle extras = new Bundle();
        extras.putString("test_key", "test_value");
        return extras;
    }

    private static boolean areBundlesEqual(Bundle b1, Bundle b2) {
        for (String key1 : b1.keySet()) {
            if (!b1.get(key1).equals(b2.get(key1))) {
                return false;
            }
        }

        for (String key2 : b2.keySet()) {
            if (!b2.get(key2).equals(b1.get(key2))) {
                return false;
            }
        }
        return true;
    }
}
