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

package com.android.internal.car;

import static android.car.test.util.UserTestingHelper.getDefaultUserType;
import static android.car.test.util.UserTestingHelper.newGuestUser;
import static android.car.test.util.UserTestingHelper.newSecondaryUser;
import static android.car.test.util.UserTestingHelper.UserInfoBuilder;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doAnswer;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.doNothing;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.mock;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.verify;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.when;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;
import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.ArgumentMatchers.nullable;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.car.test.mocks.SyncAnswer;
import android.car.userlib.CarUserManagerHelper;
import android.car.userlib.HalCallback;
import android.car.userlib.CommonConstants.CarUserServiceConstants;
import android.car.userlib.InitialUserSetter;
import android.car.userlib.UserHalHelper;
import android.car.watchdoglib.CarWatchdogDaemonHelper;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.hardware.automotive.vehicle.V2_0.UserFlags;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoRequestType;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponseAction;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.Trace;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;
import android.sysprop.CarProperties;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.car.ExternalConstants.CarUserManagerConstants;
import com.android.internal.car.ExternalConstants.ICarConstants;
import com.android.internal.os.IResultReceiver;
import com.android.server.SystemService;
import com.android.server.SystemService.TargetUser;
import com.android.server.wm.CarLaunchParamsModifier;
import com.android.server.utils.TimingsTraceAndSlog;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.quality.Strictness;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;

/**
 * This class contains unit tests for the {@link CarServiceHelperService}.
 */
@RunWith(AndroidJUnit4.class)
public class CarHelperServiceTest extends AbstractExtendedMockitoTestCase {

    private static final String TAG = CarHelperServiceTest.class.getSimpleName();

    private static final int PRE_CREATED_USER_ID = 24;
    private static final int PRE_CREATED_GUEST_ID = 25;
    private static final int USER_MANAGER_TIMEOUT_MS = 100;

    private static final String HAL_USER_NAME = "HAL 9000";
    private static final int HAL_USER_ID = 42;
    private static final int HAL_USER_FLAGS = 108;

    private static final String USER_LOCALES = "LOL";

    private static final int HAL_TIMEOUT_MS = 500;

    private static final int ADDITIONAL_TIME_MS = 200;

    private static final int HAL_NOT_REPLYING_TIMEOUT_MS = HAL_TIMEOUT_MS + ADDITIONAL_TIME_MS;

    private static final long POST_HAL_NOT_REPLYING_TIMEOUT_MS = HAL_NOT_REPLYING_TIMEOUT_MS
            + ADDITIONAL_TIME_MS;


    // Spy used in tests that need to verify folloing method:
    // managePreCreatedUsers, postAsyncPreCreatedUser, preCreateUsers
    private CarServiceHelperService mHelper;

    @Mock
    private Context mMockContext;
    @Mock
    private PackageManager mPackageManager;
    @Mock
    private Context mApplicationContext;
    @Mock
    private CarUserManagerHelper mUserManagerHelper;
    @Mock
    private UserManager mUserManager;
    @Mock
    private CarLaunchParamsModifier mCarLaunchParamsModifier;
    @Mock
    private CarWatchdogDaemonHelper mCarWatchdogDaemonHelper;
    @Mock
    private IBinder mICarBinder;
    @Mock
    private InitialUserSetter mInitialUserSetter;

    @Captor
    private ArgumentCaptor<Parcel> mBinderCallData;

    private Exception mBinderCallException;

    /**
     * Initialize objects and setup testing environment.
     */
    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session
                .spyStatic(CarProperties.class)
                .spyStatic(UserManager.class);
    }

    @Before
    public void setUpMocks() {
        mHelper = spy(new CarServiceHelperService(
                mMockContext,
                mUserManagerHelper,
                mInitialUserSetter,
                mUserManager,
                mCarLaunchParamsModifier,
                mCarWatchdogDaemonHelper,
                /* halEnabled= */ true,
                HAL_TIMEOUT_MS));

        when(mMockContext.getPackageManager()).thenReturn(mPackageManager);
    }

    @Test
    public void testCarServiceLaunched() throws Exception {
        mockRegisterReceiver();
        mockBindService();
        mockLoadLibrary();

        mHelper.onStart();

        verifyBindService();
    }

    @Test
    public void testHandleCarServiceCrash() throws Exception {
        mockHandleCarServiceCrash();
        mockCarServiceException();

        mHelper.handleCarServiceConnection(mICarBinder);

        verify(mHelper).handleCarServiceCrash();
    }

    /**
     * Test that the {@link CarServiceHelperService} starts up a secondary admin user upon first
     * run.
     */
    @Test
    public void testInitialInfo_noHal() throws Exception {
        CarServiceHelperService halLessHelper = new CarServiceHelperService(
                mMockContext,
                mUserManagerHelper,
                mInitialUserSetter,
                mUserManager,
                mCarLaunchParamsModifier,
                mCarWatchdogDaemonHelper,
                /* halEnabled= */ false,
                HAL_TIMEOUT_MS);

        halLessHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halReturnedDefault() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.DEFAULT);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halReturnedDefault_withLocale() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.DEFAULT_WITH_LOCALE);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyDefaultBootBehaviorWithLocale();
    }

    @Test
    public void testInitialInfo_halServiceNeverReturned() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.DO_NOT_REPLY);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);
        sleep("before asserting DEFAULT behavior", POST_HAL_NOT_REPLYING_TIMEOUT_MS);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isLessThan(0);

        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halServiceReturnedTooLate() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.DELAYED_REPLY);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);
        sleep("before asserting DEFAULT behavior", POST_HAL_NOT_REPLYING_TIMEOUT_MS);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        sleep("to make sure not called again", POST_HAL_NOT_REPLYING_TIMEOUT_MS);

        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halReturnedNonOkResultCode() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.NON_OK_RESULT_CODE);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halReturnedOkWithNoBundle() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.NULL_BUNDLE);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halReturnedSwitch_ok() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.SWITCH_OK);
        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyUserSwitchedByHal();
    }

    @Test
    public void testInitialInfo_halReturnedSwitch_ok_withLocale() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.SWITCH_OK_WITH_LOCALE);
        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyUserSwitchedByHalWithLocale();
    }

    @Test
    public void testInitialInfo_halReturnedSwitch_switchMissingUserId() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(InitialUserInfoAction.SWITCH_MISSING_USER_ID);

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyUserNotSwitchedByHal();
        verifyDefaultBootBehavior();
    }

    @Test
    public void testInitialInfo_halReturnedCreateOk() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo((r) -> sendCreateDefaultHalUserAction(r));

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyUserCreatedByHal();
    }

    @Test
    public void testInitialInfo_halReturnedCreateOk_withLocale() throws Exception {
        bindMockICar();

        expectICarGetInitialUserInfo(
                (r) -> sendCreateAction(r, HAL_USER_NAME, HAL_USER_FLAGS, USER_LOCALES));

        mHelper.onBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);

        assertNoICarCallExceptions();
        verifyICarGetInitialUserInfoCalled();
        assertThat(mHelper.getHalResponseTime()).isGreaterThan(0);

        verifyUserCreatedByHalWithLocale();
    }

    @Test
    public void testOnUserStarting_notifiesICar() throws Exception {
        bindMockICar();

        int userId = 10;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_STARTING,
                userId);

        mHelper.onUserStarting(newTargetUser(userId));

        assertNoICarCallExceptions();
        verifyICarOnUserLifecycleEventCalled();
    }

    @Test
    public void testOnUserStarting_preCreatedDoesntNotifyICar() throws Exception {
        bindMockICar();

        mHelper.onUserStarting(newTargetUser(10, /* preCreated= */ true));

        verifyICarOnUserLifecycleEventNeverCalled();
    }

    @Test
    public void testOnUserSwitching_notifiesICar() throws Exception {
        bindMockICar();

        int currentUserId = 10;
        int targetUserId = 11;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_SWITCHING,
                currentUserId, targetUserId);

        mHelper.onUserSwitching(newTargetUser(currentUserId),
                newTargetUser(targetUserId));

        assertNoICarCallExceptions();
    }

    @Test
    public void testOnUserSwitching_preCreatedDoesntNotifyICar() throws Exception {
        bindMockICar();

        mHelper.onUserSwitching(newTargetUser(10), newTargetUser(11, /* preCreated= */ true));

        verifyICarOnUserLifecycleEventNeverCalled();
    }

    @Test
    public void testOnUserUnlocking_notifiesICar() throws Exception {
        bindMockICar();

        int userId = 10;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_UNLOCKING,
                userId);

        mHelper.onUserUnlocking(newTargetUser(userId));

        assertNoICarCallExceptions();
    }

    @Test
    public void testOnUserUnlocking_preCreatedDoesntNotifyICar() throws Exception {
        bindMockICar();

        mHelper.onUserUnlocking(newTargetUser(10, /* preCreated= */ true));

        verifyICarOnUserLifecycleEventNeverCalled();
    }

    @Test
    public void testOnUserUnlocked_notifiesICar_systemUserFirst() throws Exception {
        bindMockICar();

        int systemUserId = UserHandle.USER_SYSTEM;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED,
                systemUserId);

        int firstUserId = 10;
        expectICarFirstUserUnlocked(firstUserId);

        setHalResponseTime();
        mHelper.onUserUnlocked(newTargetUser(systemUserId));
        mHelper.onUserUnlocked(newTargetUser(firstUserId));

        assertNoICarCallExceptions();

        verifyICarOnUserLifecycleEventCalled(); // system user
        verifyICarFirstUserUnlockedCalled();    // first user
    }

    @Test
    public void testOnUserUnlocked_notifiesICar_firstUserReportedJustOnce() throws Exception {
        bindMockICar();

        int firstUserId = 10;
        expectICarFirstUserUnlocked(firstUserId);

        int secondUserId = 11;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED,
                secondUserId);

        setHalResponseTime();
        mHelper.onUserUnlocked(newTargetUser(firstUserId));
        mHelper.onUserUnlocked(newTargetUser(secondUserId));

        assertNoICarCallExceptions();

        verifyICarFirstUserUnlockedCalled();    // first user
        verifyICarOnUserLifecycleEventCalled(); // second user
    }

    @Test
    public void testOnUserStopping_notifiesICar() throws Exception {
        bindMockICar();

        int userId = 10;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_STOPPING,
                userId);

        mHelper.onUserStopping(newTargetUser(userId));

        assertNoICarCallExceptions();
    }

    @Test
    public void testOnUserStopping_preCreatedDoesntNotifyICar() throws Exception {
        bindMockICar();

        mHelper.onUserStopping(newTargetUser(10, /* preCreated= */ true));

        verifyICarOnUserLifecycleEventNeverCalled();
    }

    @Test
    public void testOnUserStopped_notifiesICar() throws Exception {
        bindMockICar();

        int userId = 10;
        expectICarOnUserLifecycleEvent(CarUserManagerConstants.USER_LIFECYCLE_EVENT_TYPE_STOPPED,
                userId);

        mHelper.onUserStopped(newTargetUser(userId));

        assertNoICarCallExceptions();
    }

    @Test
    public void testOnUserStopped_preCreatedDoesntNotifyICar() throws Exception {
        bindMockICar();

        mHelper.onUserStopped(newTargetUser(10, /* preCreated= */ true));

        verifyICarOnUserLifecycleEventNeverCalled();
    }

    @Test
    public void testSendSetInitialUserInfoNotifiesICar() throws Exception {
        bindMockICar();

        UserInfo user = new UserInfo(42, "Dude", UserInfo.FLAG_ADMIN);
        expectICarSetInitialUserInfo(user);

        mHelper.setInitialUser(user);

        verifyICarSetInitialUserCalled();
        assertNoICarCallExceptions();
    }

    @Test
    public void testInitialUserInfoRequestType_FirstBoot() throws Exception {
        when(mUserManagerHelper.hasInitialUser()).thenReturn(false);
        when(mPackageManager.isDeviceUpgrading()).thenReturn(true);
        assertThat(mHelper.getInitialUserInfoRequestType())
                .isEqualTo(InitialUserInfoRequestType.FIRST_BOOT);
    }

    @Test
    public void testInitialUserInfoRequestType_FirstBootAfterOTA() throws Exception {
        when(mUserManagerHelper.hasInitialUser()).thenReturn(true);
        when(mPackageManager.isDeviceUpgrading()).thenReturn(true);
        assertThat(mHelper.getInitialUserInfoRequestType())
                .isEqualTo(InitialUserInfoRequestType.FIRST_BOOT_AFTER_OTA);
    }

    @Test
    public void testInitialUserInfoRequestType_ColdBoot() throws Exception {
        when(mUserManagerHelper.hasInitialUser()).thenReturn(true);
        when(mPackageManager.isDeviceUpgrading()).thenReturn(false);
        assertThat(mHelper.getInitialUserInfoRequestType())
                .isEqualTo(InitialUserInfoRequestType.COLD_BOOT);
    }

    @Test
    public void testPreCreatedUsersLessThanRequested() throws Exception {
        // Set existing user
        expectNoPreCreatedUser();
        // Set number of requested user
        setNumberRequestedUsersProperty(1);
        setNumberRequestedGuestsProperty(0);
        mockRunAsync();
        SyncAnswer syncUserInfo = mockPreCreateUser(/* isGuest= */ false);

        mHelper.managePreCreatedUsers();
        syncUserInfo.await(USER_MANAGER_TIMEOUT_MS);

        verifyUserCreated(/* isGuest= */ false);
    }

    @Test
    public void testPreCreatedGuestsLessThanRequested() throws Exception {
        // Set existing user
        expectNoPreCreatedUser();
        // Set number of requested user
        setNumberRequestedUsersProperty(0);
        setNumberRequestedGuestsProperty(1);
        mockRunAsync();
        SyncAnswer syncUserInfo = mockPreCreateUser(/* isGuest= */ true);

        mHelper.managePreCreatedUsers();
        syncUserInfo.await(USER_MANAGER_TIMEOUT_MS);

        verifyUserCreated(/* isGuest= */ true);
    }

    @Test
    public void testRemovePreCreatedUser() throws Exception {
        UserInfo user = expectPreCreatedUser(/* isGuest= */ false,
                /* isInitialized= */ true);
        setNumberRequestedUsersProperty(0);
        setNumberRequestedGuestsProperty(0);
        mockRunAsync();

        SyncAnswer syncRemoveStatus = mockRemoveUser(PRE_CREATED_USER_ID);

        mHelper.managePreCreatedUsers();
        syncRemoveStatus.await(USER_MANAGER_TIMEOUT_MS);

        verifyUserRemoved(user);
    }

    @Test
    public void testRemovePreCreatedGuest() throws Exception {
        UserInfo user = expectPreCreatedUser(/* isGuest= */ true,
                /* isInitialized= */ true);
        setNumberRequestedUsersProperty(0);
        setNumberRequestedGuestsProperty(0);
        mockRunAsync();
        SyncAnswer syncRemoveStatus = mockRemoveUser(PRE_CREATED_GUEST_ID);

        mHelper.managePreCreatedUsers();
        syncRemoveStatus.await(USER_MANAGER_TIMEOUT_MS);

        verifyUserRemoved(user);
    }

    @Test
    public void testRemoveInvalidPreCreatedUser() throws Exception {
        UserInfo user = expectPreCreatedUser(/* isGuest= */ false,
                /* isInitialized= */ false);
        setNumberRequestedUsersProperty(0);
        setNumberRequestedGuestsProperty(0);
        mockRunAsync();
        SyncAnswer syncRemoveStatus = mockRemoveUser(PRE_CREATED_USER_ID);

        mHelper.managePreCreatedUsers();
        syncRemoveStatus.await(ADDITIONAL_TIME_MS);

        verifyUserRemoved(user);
    }

    @Test
    public void testManagePreCreatedUsersDoNothing() throws Exception {
        UserInfo user = expectPreCreatedUser(/* isGuest= */ false,
                /* isInitialized= */ true);
        setNumberRequestedUsersProperty(1);
        setNumberRequestedGuestsProperty(0);
        mockPreCreateUser(/* isGuest= */ false);
        mockRemoveUser(PRE_CREATED_USER_ID);

        mHelper.managePreCreatedUsers();

        verifyPostPreCreatedUserSkipped();
    }

    @Test
    public void testManagePreCreatedUsersOnBootCompleted() throws Exception {
        mockRunAsync();

        mHelper.onBootPhase(SystemService.PHASE_BOOT_COMPLETED);

        verifyManagePreCreatedUsers();
    }

    @Test
    public void testPreCreateUserExceptionLogged() throws Exception {
        SyncAnswer syncException = mockPreCreateUserException();
        TimingsTraceAndSlog trace = new TimingsTraceAndSlog(TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
        mHelper.preCreateUsers(trace, false);

        verifyPostPreCreatedUserException();
        assertThat(trace.getUnfinishedTracesForDebug()).isEmpty();
    }

    private void setHalResponseTime() {
        mHelper.setInitialHalResponseTime();
        SystemClock.sleep(1); // must sleep at least 1ms so it's not 0
        mHelper.setFinalHalResponseTime();
    }

    /**
     * Used in cases where the result of calling HAL for the initial info should be the same as
     * not using HAL.
     */
    private void verifyDefaultBootBehavior() throws Exception {
        verify(mInitialUserSetter).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_DEFAULT_BEHAVIOR && info.userLocales == null;
        }));
    }

    private void verifyDefaultBootBehaviorWithLocale() {
        verify(mInitialUserSetter).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_DEFAULT_BEHAVIOR
                    && USER_LOCALES.equals(info.userLocales);
        }));
    }

    private TargetUser newTargetUser(int userId) {
        return newTargetUser(userId, /* preCreated= */ false);
    }

    private TargetUser newTargetUser(int userId, boolean preCreated) {
        TargetUser targetUser = mock(TargetUser.class);
        when(targetUser.getUserIdentifier()).thenReturn(userId);
        UserInfo userInfo = new UserInfo();
        userInfo.id = userId;
        userInfo.preCreated = preCreated;
        when(targetUser.getUserInfo()).thenReturn(userInfo);
        return targetUser;
    }

    private void bindMockICar() throws Exception {
        // Must set the binder expectation, otherwise checks for other transactions would fail
        expectICarSetCarServiceHelper();
        mHelper.handleCarServiceConnection(mICarBinder);
    }

    private void verifyUserCreatedByHal() throws Exception {
        verify(mInitialUserSetter).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_CREATE
                    && info.newUserName == HAL_USER_NAME
                    && info.newUserFlags == HAL_USER_FLAGS
                    && info.userLocales == null;
        }));
    }

    private void verifyUserCreatedByHalWithLocale() throws Exception {
        verify(mInitialUserSetter).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_CREATE
                    && info.newUserName == HAL_USER_NAME
                    && info.newUserFlags == HAL_USER_FLAGS
                    && info.userLocales == USER_LOCALES;
        }));
    }

    private void verifyUserSwitchedByHal() {
        verify(mInitialUserSetter).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_SWITCH
                    && info.switchUserId == HAL_USER_ID
                    && info.userLocales == null;
        }));
    }

    private void verifyUserSwitchedByHalWithLocale() {
        verify(mInitialUserSetter).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_SWITCH
                    && info.switchUserId == HAL_USER_ID
                    && info.userLocales == USER_LOCALES;
        }));
    }

    private void verifyUserNotSwitchedByHal() {
        verify(mInitialUserSetter, never()).set(argThat((info) -> {
            return info.type == InitialUserSetter.TYPE_SWITCH;
        }));
    }

    private void verifyBindService () throws Exception {
        verify(mMockContext).bindServiceAsUser(
                argThat(intent -> intent.getAction().equals(ICarConstants.CAR_SERVICE_INTERFACE)),
                any(), eq(Context.BIND_AUTO_CREATE), eq(UserHandle.SYSTEM));
    }

    private void verifyHandleCarServiceCrash() throws Exception {
        verify(mHelper).handleCarServiceCrash();
    }

    private void mockRegisterReceiver() {
        when(mMockContext.registerReceiverForAllUsers(any(), any(), any(), any()))
                .thenReturn(new Intent());
    }

    private void mockBindService() {
        when(mMockContext.bindServiceAsUser(any(), any(),
                eq(Context.BIND_AUTO_CREATE), eq(UserHandle.SYSTEM)))
                .thenReturn(true);
    }

    private void mockLoadLibrary() {
        doNothing().when(mHelper).loadNativeLibrary();
    }

    private void mockCarServiceException() throws Exception {
        when(mICarBinder.transact(anyInt(), notNull(), isNull(), eq(Binder.FLAG_ONEWAY)))
                .thenThrow(new RemoteException("mock car service Crash"));
    }

    private void mockHandleCarServiceCrash() throws Exception {
        doNothing().when(mHelper).handleCarServiceCrash();
    }

    // TODO: create a custom matcher / verifier for binder calls
    private void expectICarOnUserLifecycleEvent(int eventType, int expectedUserId)
            throws Exception {
        expectICarOnUserLifecycleEvent(eventType, UserHandle.USER_NULL, expectedUserId);
    }

    private void expectICarSetCarServiceHelper() throws Exception {
        int txn = IBinder.FIRST_CALL_TRANSACTION
                + ICarConstants.ICAR_CALL_SET_CAR_SERVICE_HELPER;
        when(mICarBinder.transact(eq(txn), notNull(), isNull(), eq(Binder.FLAG_ONEWAY)))
                .thenReturn(true);
    }

    private void expectICarOnUserLifecycleEvent(int expectedEventType, int expectedFromUserId,
            int expectedToUserId) throws Exception {
        int txn = IBinder.FIRST_CALL_TRANSACTION + ICarConstants.ICAR_CALL_ON_USER_LIFECYCLE;
        long before = System.currentTimeMillis();

        when(mICarBinder.transact(eq(txn), notNull(), isNull(),
                eq(Binder.FLAG_ONEWAY))).thenAnswer((invocation) -> {
                    try {
                        long after = System.currentTimeMillis();
                        Log.d(TAG, "Answering txn " + txn);
                        Parcel data = (Parcel) invocation.getArguments()[1];
                        data.setDataPosition(0);
                        data.enforceInterface(ICarConstants.CAR_SERVICE_INTERFACE);
                        int actualEventType = data.readInt();
                        long actualTimestamp = data.readLong();
                        int actualFromUserId = data.readInt();
                        int actualToUserId = data.readInt();
                        Log.d(TAG, "Unmarshalled data: eventType=" + actualEventType
                                + ", timestamp= " + actualTimestamp
                                + ", fromUserId= " + actualFromUserId
                                + ", toUserId= " + actualToUserId);
                        List<String> errors = new ArrayList<>();
                        assertParcelValueInRange(errors, "timestamp", before, actualTimestamp, after);
                        assertParcelValue(errors, "eventType", expectedEventType, actualEventType);
                        assertParcelValue(errors, "fromUserId", expectedFromUserId,
                                actualFromUserId);
                        assertParcelValue(errors, "toUserId", expectedToUserId, actualToUserId);
                        assertNoParcelErrors(errors);
                        return true;
                    } catch (Exception e) {
                        Log.e(TAG, "Exception answering binder call", e);
                        mBinderCallException = e;
                        return false;
                    }
                });
    }

    private void expectICarFirstUserUnlocked(int expectedUserId) throws Exception {
        int txn = IBinder.FIRST_CALL_TRANSACTION + ICarConstants.ICAR_CALL_FIRST_USER_UNLOCKED;
        long before = System.currentTimeMillis();
        long minDuration = SystemClock.elapsedRealtime() - Process.getStartElapsedRealtime();

        when(mICarBinder.transact(eq(txn), notNull(), isNull(),
                eq(Binder.FLAG_ONEWAY))).thenAnswer((invocation) -> {
                    try {
                        long after = System.currentTimeMillis();
                        Log.d(TAG, "Answering txn " + txn);
                        Parcel data = (Parcel) invocation.getArguments()[1];
                        data.setDataPosition(0);
                        data.enforceInterface(ICarConstants.CAR_SERVICE_INTERFACE);
                        int actualUserId = data.readInt();
                        long actualTimestamp = data.readLong();
                        long actualDuration = data.readLong();
                        int actualHalResponseTime = data.readInt();
                        Log.d(TAG, "Unmarshalled data: userId= " + actualUserId
                                + ", timestamp= " + actualTimestamp
                                + ", duration=" + actualDuration
                                + ", halResponseTime=" + actualHalResponseTime);
                        List<String> errors = new ArrayList<>();
                        assertParcelValue(errors, "userId", expectedUserId, actualUserId);
                        assertParcelValueInRange(errors, "timestamp", before, actualTimestamp,
                                after);
                        assertMinimumParcelValue(errors, "duration", minDuration, actualDuration);
                        assertMinimumParcelValue(errors, "halResponseTime", 1, actualHalResponseTime);
                        assertNoParcelErrors(errors);
                        return true;
                    } catch (Exception e) {
                        Log.e(TAG, "Exception answering binder call", e);
                        mBinderCallException = e;
                        return false;
                    }
                });

    }

    enum InitialUserInfoAction {
        DEFAULT,
        DEFAULT_WITH_LOCALE,
        DO_NOT_REPLY,
        DELAYED_REPLY,
        NON_OK_RESULT_CODE,
        NULL_BUNDLE,
        SWITCH_OK,
        SWITCH_OK_WITH_LOCALE,
        SWITCH_MISSING_USER_ID
    }

    private void expectICarGetInitialUserInfo(InitialUserInfoAction action) throws Exception {
        expectICarGetInitialUserInfo((receiver) ->{
            switch (action) {
                case DEFAULT:
                    sendDefaultAction(receiver);
                    break;
                case DEFAULT_WITH_LOCALE:
                    sendDefaultAction(receiver, USER_LOCALES);
                    break;
                case DO_NOT_REPLY:
                    Log.d(TAG, "NOT replying to bind call");
                    break;
                case DELAYED_REPLY:
                    sleep("before sending result", HAL_NOT_REPLYING_TIMEOUT_MS);
                    sendDefaultAction(receiver);
                    break;
                case NON_OK_RESULT_CODE:
                    Log.d(TAG, "sending bad result code");
                    receiver.send(-1, null);
                    break;
                case NULL_BUNDLE:
                    Log.d(TAG, "sending OK without bundle");
                    receiver.send(HalCallback.STATUS_OK, null);
                    break;
                case SWITCH_OK:
                    sendValidSwitchAction(receiver, /* userLocales= */ null);
                    break;
                case SWITCH_OK_WITH_LOCALE:
                    sendValidSwitchAction(receiver, USER_LOCALES);
                    break;
                case SWITCH_MISSING_USER_ID:
                    Log.d(TAG, "sending Switch without user Id");
                    sendSwitchAction(receiver, /* id= */ null, /* userLocales= */ null);
                    break;
               default:
                    throw new IllegalArgumentException("invalid action: " + action);
            }
        });
    }

    private void expectICarGetInitialUserInfo(GetInitialUserInfoAction action) throws Exception {
        int txn = IBinder.FIRST_CALL_TRANSACTION + ICarConstants.ICAR_CALL_GET_INITIAL_USER_INFO;
        when(mICarBinder.transact(eq(txn), notNull(), isNull(),
                eq(Binder.FLAG_ONEWAY))).thenAnswer((invocation) -> {
                    try {
                        Log.d(TAG, "Answering txn " + txn);
                        Parcel data = (Parcel) invocation.getArguments()[1];
                        data.setDataPosition(0);
                        data.enforceInterface(ICarConstants.CAR_SERVICE_INTERFACE);
                        int actualRequestType = data.readInt();
                        int actualTimeoutMs = data.readInt();
                        IResultReceiver receiver = IResultReceiver.Stub
                                .asInterface(data.readStrongBinder());

                        Log.d(TAG, "Unmarshalled data: requestType= " + actualRequestType
                                + ", timeout=" + actualTimeoutMs
                                + ", receiver =" + receiver);
                        action.onReceiver(receiver);
                        return true;
                    } catch (Exception e) {
                        Log.e(TAG, "Exception answering binder call", e);
                        mBinderCallException = e;
                        return false;
                    }
                });
    }

    private void expectICarSetInitialUserInfo(UserInfo user) throws RemoteException {
        int txn = IBinder.FIRST_CALL_TRANSACTION + ICarConstants.ICAR_CALL_SET_INITIAL_USER;
        when(mICarBinder.transact(eq(txn), notNull(), isNull(),
                eq(Binder.FLAG_ONEWAY))).thenAnswer((invocation) -> {
                    try {
                        Log.d(TAG, "Answering txn " + txn);
                        Parcel data = (Parcel) invocation.getArguments()[1];
                        data.setDataPosition(0);
                        data.enforceInterface(ICarConstants.CAR_SERVICE_INTERFACE);
                        int actualUserId = data.readInt();
                        Log.d(TAG, "Unmarshalled data: user= " + actualUserId);
                        assertThat(actualUserId).isEqualTo(user.id);
                        return true;
                    } catch (Exception e) {
                        Log.e(TAG, "Exception answering binder call", e);
                        mBinderCallException = e;
                        return false;
                    }
                });
    }

    private void expectNoPreCreatedUser() throws Exception {
        when(mUserManager.getUsers(/* excludePartial= */ true,
                /* excludeDying= */ true, /* excludePreCreated= */ false))
                .thenReturn(new ArrayList<UserInfo> ());
    }

    private UserInfo expectPreCreatedUser(boolean isGuest, boolean isInitialized)
            throws Exception {
        int userId = isGuest ? PRE_CREATED_GUEST_ID : PRE_CREATED_USER_ID;
        UserInfo user = new UserInfoBuilder(userId)
                .setGuest(isGuest)
                .setPreCreated(true)
                .setInitialized(isInitialized)
                .build();

        when(mUserManager.getUsers(/* excludePartial= */ true,
                /* excludeDying= */ true, /* excludePreCreated= */ false))
                .thenReturn(Arrays.asList(user));
        return user;
    }

    private interface GetInitialUserInfoAction {
        void onReceiver(IResultReceiver receiver) throws Exception;
    }

    private void sendDefaultAction(IResultReceiver receiver) throws Exception {
        sendDefaultAction(receiver, /* userLocales= */ null);
    }

    private void sendDefaultAction(IResultReceiver receiver, String userLocales) throws Exception {
        Log.d(TAG, "Sending DEFAULT action to receiver " + receiver);
        Bundle data = new Bundle();
        data.putInt(CarUserServiceConstants.BUNDLE_INITIAL_INFO_ACTION,
                InitialUserInfoResponseAction.DEFAULT);
        if (userLocales != null) {
            data.putString(CarUserServiceConstants.BUNDLE_USER_LOCALES, userLocales);
        }
        receiver.send(HalCallback.STATUS_OK, data);
    }

    private void sendValidSwitchAction(IResultReceiver receiver, String userLocales)
            throws Exception {
        Log.d(TAG, "Sending SWITCH (" + HAL_USER_ID + ") action to receiver " + receiver);
        sendSwitchAction(receiver, HAL_USER_ID, userLocales);
    }

    private void sendSwitchAction(IResultReceiver receiver, Integer id, String userLocales)
            throws Exception {
        Bundle data = new Bundle();
        data.putInt(CarUserServiceConstants.BUNDLE_INITIAL_INFO_ACTION,
                InitialUserInfoResponseAction.SWITCH);
        if (id != null) {
            data.putInt(CarUserServiceConstants.BUNDLE_USER_ID, id);
        }
        if (userLocales != null) {
            data.putString(CarUserServiceConstants.BUNDLE_USER_LOCALES, userLocales);
        }
        receiver.send(HalCallback.STATUS_OK, data);
    }

    private void sendCreateDefaultHalUserAction(IResultReceiver receiver) throws Exception {
        sendCreateAction(receiver, HAL_USER_NAME, HAL_USER_FLAGS, /* userLocales= */ null);
    }

    private void sendCreateAction(IResultReceiver receiver, String name, Integer flags,
            String userLocales) throws Exception {
        Bundle data = new Bundle();
        data.putInt(CarUserServiceConstants.BUNDLE_INITIAL_INFO_ACTION,
                InitialUserInfoResponseAction.CREATE);
        if (name != null) {
            data.putString(CarUserServiceConstants.BUNDLE_USER_NAME, name);
        }
        if (flags != null) {
            data.putInt(CarUserServiceConstants.BUNDLE_USER_FLAGS, flags);
        }
        if (userLocales != null) {
            data.putString(CarUserServiceConstants.BUNDLE_USER_LOCALES, userLocales);
        }
        receiver.send(HalCallback.STATUS_OK, data);
    }

    private void sleep(String reason, long napTimeMs) {
        Log.d(TAG, "Sleeping for " + napTimeMs + "ms: " + reason);
        SystemClock.sleep(napTimeMs);
        Log.d(TAG, "Woke up (from '"  + reason + "')");
    }

    private void verifyICarOnUserLifecycleEventCalled() throws Exception {
        verifyICarTxnCalled(ICarConstants.ICAR_CALL_ON_USER_LIFECYCLE);
    }

    private void verifyICarOnUserLifecycleEventNeverCalled() throws Exception {
        verifyICarTxnNeverCalled(ICarConstants.ICAR_CALL_ON_USER_LIFECYCLE);
    }

    private void verifyICarFirstUserUnlockedCalled() throws Exception {
        verifyICarTxnCalled(ICarConstants.ICAR_CALL_FIRST_USER_UNLOCKED);
    }

    private void verifyICarGetInitialUserInfoCalled() throws Exception {
        verifyICarTxnCalled(ICarConstants.ICAR_CALL_GET_INITIAL_USER_INFO);
    }

    private void verifyICarSetInitialUserCalled() throws Exception {
        verifyICarTxnCalled(ICarConstants.ICAR_CALL_SET_INITIAL_USER);
    }

    private void verifyICarTxnCalled(int txnId) throws Exception {
        int txn = IBinder.FIRST_CALL_TRANSACTION + txnId;
        verify(mICarBinder).transact(eq(txn), notNull(), isNull(), eq(Binder.FLAG_ONEWAY));
    }

    private void verifyICarTxnNeverCalled(int txnId) throws Exception {
        int txn = IBinder.FIRST_CALL_TRANSACTION + txnId;
        verify(mICarBinder, never()).transact(eq(txn), notNull(), isNull(), eq(Binder.FLAG_ONEWAY));
    }

    private void setNumberRequestedUsersProperty(int numberUser) {
        doReturn(Optional.of(numberUser)).when(() -> CarProperties.number_pre_created_users());
    }

    private void setNumberRequestedGuestsProperty(int numberGuest) {
        doReturn(Optional.of(numberGuest)).when(() -> CarProperties.number_pre_created_guests());
    }

    private void mockRunAsync() {
        doAnswer(answerVoid(Runnable::run)).when(mHelper).runAsync(any(Runnable.class));
    }

    private SyncAnswer mockPreCreateUser(boolean isGuest) {
        UserInfo newUser = isGuest ? newGuestUser(PRE_CREATED_GUEST_ID) :
                newSecondaryUser(PRE_CREATED_USER_ID);
        SyncAnswer<UserInfo> syncUserInfo = SyncAnswer.forReturn(newUser);
        when(mUserManager.preCreateUser(getDefaultUserType(isGuest)))
                .thenAnswer(syncUserInfo);

        return syncUserInfo;
    }

    private SyncAnswer mockRemoveUser(@UserIdInt int userId) {
        SyncAnswer<Boolean> syncRemoveStatus = SyncAnswer.forReturn(true);
        when(mUserManager.removeUser(userId)).thenAnswer(syncRemoveStatus);

        return syncRemoveStatus;
    }

    private SyncAnswer mockPreCreateUserException() {
        SyncAnswer<UserInfo> syncException = SyncAnswer.forException(new Exception());
        when(mUserManager.preCreateUser(anyString()))
                .thenAnswer(syncException);
        return syncException;
    }

    private void verifyUserCreated(boolean isGuest) throws Exception {
        String userType =
                isGuest ? UserManager.USER_TYPE_FULL_GUEST : UserManager.USER_TYPE_FULL_SECONDARY;
        verify(mUserManager).preCreateUser(eq(userType));
    }

    private void verifyUserRemoved(UserInfo user) throws Exception {
        verify(mUserManager).removeUser(user.id);
    }

    private void verifyPostPreCreatedUserSkipped() throws Exception {
        verify(mHelper, never()).runAsync(any());
    }

    private void verifyPostPreCreatedUserException() throws Exception {
        verify(mHelper).logPrecreationFailure(anyString(), any());
    }

    private void verifyManagePreCreatedUsers() throws Exception {
        verify(mHelper).managePreCreatedUsers();
    }

    private void assertParcelValue(List<String> errors, String field, int expected,
            int actual) {
        if (expected != actual) {
            errors.add(String.format("%s mismatch: expected=%d, actual=%d",
                    field, expected, actual));
        }
    }

    private void assertParcelValueInRange(List<String> errors, String field, long before,
            long actual, long after) {
        if (actual < before || actual> after) {
            errors.add(field + " (" + actual+ ") not in range [" + before + ", " + after + "]");
        }
    }

    private void assertMinimumParcelValue(List<String> errors, String field, long min,
            long actual) {
        if (actual < min) {
            errors.add("Minimum " + field + " should be " + min + " (was " + actual + ")");
        }
    }

    private void assertNoParcelErrors(List<String> errors) {
        int size = errors.size();
        if (size == 0) return;

        StringBuilder msg = new StringBuilder().append(size).append(" errors on parcel: ");
        for (String error : errors) {
            msg.append("\n\t").append(error);
        }
        msg.append('\n');
        throw new IllegalArgumentException(msg.toString());
    }

    /**
     * Asserts that no exception was thrown when answering to a mocked {@code ICar} binder call.
     * <p>
     * This method should be called before asserting the expected results of a test, so it makes
     * clear why the test failed when the call was not made as expected.
     */
    private void assertNoICarCallExceptions() throws Exception {
        if (mBinderCallException != null)
            throw mBinderCallException;

    }
}
