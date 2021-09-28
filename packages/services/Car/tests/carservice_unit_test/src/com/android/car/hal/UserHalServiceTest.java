/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.car.hal;

import static android.car.VehiclePropertyIds.CREATE_USER;
import static android.car.VehiclePropertyIds.CURRENT_GEAR;
import static android.car.VehiclePropertyIds.INITIAL_USER_INFO;
import static android.car.VehiclePropertyIds.REMOVE_USER;
import static android.car.VehiclePropertyIds.SWITCH_USER;
import static android.car.VehiclePropertyIds.USER_IDENTIFICATION_ASSOCIATION;
import static android.car.test.mocks.CarArgumentMatchers.isProperty;
import static android.car.test.mocks.CarArgumentMatchers.isPropertyWithValues;
import static android.car.test.util.VehicleHalTestingHelper.newConfig;
import static android.car.test.util.VehicleHalTestingHelper.newSubscribableConfig;
import static android.hardware.automotive.vehicle.V2_0.InitialUserInfoRequestType.COLD_BOOT;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.ASSOCIATE_CURRENT_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_1;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.KEY_FOB;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue.ASSOCIATED_CURRENT_USER;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.annotation.NonNull;
import android.car.hardware.property.VehicleHalStatusCode;
import android.car.userlib.HalCallback;
import android.car.userlib.UserHalHelper;
import android.hardware.automotive.vehicle.V2_0.CreateUserRequest;
import android.hardware.automotive.vehicle.V2_0.CreateUserResponse;
import android.hardware.automotive.vehicle.V2_0.CreateUserStatus;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponse;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponseAction;
import android.hardware.automotive.vehicle.V2_0.RemoveUserRequest;
import android.hardware.automotive.vehicle.V2_0.SwitchUserMessageType;
import android.hardware.automotive.vehicle.V2_0.SwitchUserRequest;
import android.hardware.automotive.vehicle.V2_0.SwitchUserResponse;
import android.hardware.automotive.vehicle.V2_0.SwitchUserStatus;
import android.hardware.automotive.vehicle.V2_0.UserFlags;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociation;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationGetRequest;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationResponse;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationSetAssociation;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationSetRequest;
import android.hardware.automotive.vehicle.V2_0.UserInfo;
import android.hardware.automotive.vehicle.V2_0.UsersInfo;
import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.os.Handler;
import android.os.Looper;
import android.os.ServiceSpecificException;
import android.os.SystemClock;
import android.os.UserHandle;
import android.util.Log;
import android.util.Pair;

import com.android.car.CarLocalServices;
import com.android.car.user.CarUserService;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(MockitoJUnitRunner.class)
public final class UserHalServiceTest {

    private static final String TAG = UserHalServiceTest.class.getSimpleName();

    /**
     * Timeout passed to {@link UserHalService} methods
     */
    private static final int TIMEOUT_MS = 50;

    /**
     * Timeout for {@link GenericHalCallback#assertCalled()} for tests where the HAL is supposed to
     * return something - it's a short time so it doesn't impact the test duration.
     */
    private static final int CALLBACK_TIMEOUT_SUCCESS = TIMEOUT_MS + 50;

    /**
     * Timeout for {@link GenericHalCallback#assertCalled()} for tests where the HAL is not supposed
     * to return anything - it's a slightly longer to make sure the test doesn't fail prematurely.
     */
    private static final int CALLBACK_TIMEOUT_TIMEOUT = TIMEOUT_MS + 450;

    // Used when crafting a request property - the real value will be set by the mock.
    private static final int REQUEST_ID_PLACE_HOLDER = 1111;

    private static final int DEFAULT_REQUEST_ID = 2222;

    private static final int DEFAULT_USER_ID = 333;
    private static final int DEFAULT_USER_FLAGS = 444;

    private static final int INITIAL_USER_INFO_RESPONSE_ACTION = 108;

    @Mock
    private VehicleHal mVehicleHal;
    @Mock
    private CarUserService mCarUserService;

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final UserInfo mUser0 = new UserInfo();
    private final UserInfo mUser10 = new UserInfo();

    private final UsersInfo mUsersInfo = new UsersInfo();

    // Must be a spy so we can mock getNextRequestId()
    private UserHalService mUserHalService;

    @Before
    public void setFixtures() {
        mUserHalService = spy(new UserHalService(mVehicleHal, mHandler));
        // Needs at least one property, otherwise isSupported() and isUserAssociationSupported()
        // will return false
        mUserHalService.takeProperties(Arrays.asList(newSubscribableConfig(INITIAL_USER_INFO),
                newSubscribableConfig(CREATE_USER), newSubscribableConfig(REMOVE_USER),
                newSubscribableConfig(SWITCH_USER),
                newSubscribableConfig(USER_IDENTIFICATION_ASSOCIATION)));

        mUser0.userId = 0;
        mUser0.flags = 100;
        mUser10.userId = 10;
        mUser10.flags = 110;

        mUsersInfo.currentUser = mUser0;
        mUsersInfo.numberUsers = 2;
        mUsersInfo.existingUsers = new ArrayList<>(2);
        mUsersInfo.existingUsers.add(mUser0);
        mUsersInfo.existingUsers.add(mUser10);

        CarLocalServices.addService(CarUserService.class, mCarUserService);
    }

    @After
    public void clearFixtures() {
        CarLocalServices.removeServiceForTest(CarUserService.class);
    }

    @Test
    public void testTakeSupportedProperties_supportedNoProperties() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        myHalService.takeProperties(Collections.emptyList());
        assertThat(myHalService.isSupported()).isFalse();
        assertThat(myHalService.isUserAssociationSupported()).isFalse();
    }

    @Test
    public void testTakeSupportedProperties_supportedFewProperties() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);
        myHalService.takeProperties(Arrays.asList(newSubscribableConfig(INITIAL_USER_INFO),
                newSubscribableConfig(CREATE_USER), newSubscribableConfig(REMOVE_USER)));

        assertThat(myHalService.isSupported()).isFalse();
        assertThat(myHalService.isUserAssociationSupported()).isFalse();
    }

    @Test
    public void testTakeSupportedProperties_supportedAllCoreProperties() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);
        myHalService.takeProperties(Arrays.asList(newSubscribableConfig(INITIAL_USER_INFO),
                newSubscribableConfig(CREATE_USER), newSubscribableConfig(REMOVE_USER),
                newSubscribableConfig(SWITCH_USER)));

        assertThat(myHalService.isSupported()).isTrue();
        assertThat(myHalService.isUserAssociationSupported()).isFalse();
    }

    @Test
    public void testTakeSupportedProperties_supportedAllProperties() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);
        myHalService.takeProperties(Arrays.asList(newSubscribableConfig(INITIAL_USER_INFO),
                newSubscribableConfig(CREATE_USER), newSubscribableConfig(REMOVE_USER),
                newSubscribableConfig(SWITCH_USER),
                newSubscribableConfig(USER_IDENTIFICATION_ASSOCIATION)));

        assertThat(myHalService.isSupported()).isTrue();
        assertThat(myHalService.isUserAssociationSupported()).isTrue();
    }

    @Test
    public void testTakeSupportedPropertiesAndInit() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);
        VehiclePropConfig unsupportedConfig = newConfig(CURRENT_GEAR);

        myHalService.takeProperties(Arrays.asList(newSubscribableConfig(INITIAL_USER_INFO),
                newSubscribableConfig(CREATE_USER), newSubscribableConfig(REMOVE_USER),
                newSubscribableConfig(SWITCH_USER), unsupportedConfig,
                newSubscribableConfig(USER_IDENTIFICATION_ASSOCIATION)));


        // Ideally there should be 2 test methods (one for takeSupportedProperties() and one for
        // init()), but on "real life" VehicleHal calls these 2 methods in sequence, and the latter
        // depends on the properties set by the former, so it's ok to test both here...
        myHalService.init();
        verify(mVehicleHal).subscribeProperty(myHalService, INITIAL_USER_INFO);
        verify(mVehicleHal).subscribeProperty(myHalService, CREATE_USER);
        verify(mVehicleHal).subscribeProperty(myHalService, REMOVE_USER);
        verify(mVehicleHal).subscribeProperty(myHalService, SWITCH_USER);
        verify(mVehicleHal).subscribeProperty(myHalService, USER_IDENTIFICATION_ASSOCIATION);
    }

    @Test
    public void testSupportedProperties() {
        assertThat(mUserHalService.getAllSupportedProperties()).asList().containsExactly(
                INITIAL_USER_INFO, CREATE_USER, REMOVE_USER, SWITCH_USER,
                USER_IDENTIFICATION_ASSOCIATION);
    }

    @Test
    public void testGetUserInfo_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class, () -> myHalService.getInitialUserInfo(COLD_BOOT,
                TIMEOUT_MS, mUsersInfo, noOpCallback()));
    }

    @Test
    public void testGetUserInfo_invalidTimeout() {
        assertThrows(IllegalArgumentException.class, () ->
                mUserHalService.getInitialUserInfo(COLD_BOOT, 0, mUsersInfo, noOpCallback()));
        assertThrows(IllegalArgumentException.class, () ->
                mUserHalService.getInitialUserInfo(COLD_BOOT, -1, mUsersInfo, noOpCallback()));
    }

    @Test
    public void testGetUserInfo_noUsersInfo() {
        assertThrows(NullPointerException.class, () ->
                mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, null, noOpCallback()));
    }

    @Test
    public void testGetUserInfo_noCallback() {
        assertThrows(NullPointerException.class,
                () -> mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS,
                        mUsersInfo, null));
    }

    @Test
    public void testGetUserInfo_halSetTimedOut() throws Exception {
        replySetPropertyWithTimeoutException(INITIAL_USER_INFO);

        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_SET_TIMEOUT);
        assertThat(callback.response).isNull();

        // Make sure the pending request was removed
        SystemClock.sleep(CALLBACK_TIMEOUT_TIMEOUT);
        callback.assertNotCalledAgain();
    }

    @Test
    public void testGetUserInfo_halDidNotReply() throws Exception {
        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testGetUserInfo_secondCallFailWhilePending() throws Exception {
        GenericHalCallback<InitialUserInfoResponse> callback1 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        GenericHalCallback<InitialUserInfoResponse> callback2 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback1);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback2);

        callback1.assertCalled();
        assertCallbackStatus(callback1, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback1.response).isNull();

        callback2.assertCalled();
        assertCallbackStatus(callback2, HalCallback.STATUS_CONCURRENT_OPERATION);
        assertThat(callback1.response).isNull();
    }

    @Test
    public void testGetUserInfo_halReplyWithWrongRequestId() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(INITIAL_USER_INFO,
                    REQUEST_ID_PLACE_HOLDER, INITIAL_USER_INFO_RESPONSE_ACTION);

        replySetPropertyWithOnChangeEvent(INITIAL_USER_INFO, propResponse,
                /* rightRequestId= */ false);

        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testGetUserInfo_halReturnedInvalidAction() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(INITIAL_USER_INFO,
                    REQUEST_ID_PLACE_HOLDER, INITIAL_USER_INFO_RESPONSE_ACTION);

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                INITIAL_USER_INFO, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertInitialUserInfoSetRequest(reqCaptor.get(), COLD_BOOT);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testGetUserInfo_successDefault() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(INITIAL_USER_INFO,
                    REQUEST_ID_PLACE_HOLDER, InitialUserInfoResponseAction.DEFAULT);

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                INITIAL_USER_INFO, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertInitialUserInfoSetRequest(reqCaptor.get(), COLD_BOOT);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        InitialUserInfoResponse actualResponse = callback.response;
        assertThat(actualResponse.action).isEqualTo(InitialUserInfoResponseAction.DEFAULT);
        assertThat(actualResponse.userNameToCreate).isEmpty();
        assertThat(actualResponse.userToSwitchOrCreate).isNotNull();
        assertThat(actualResponse.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(actualResponse.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
    }

    @Test
    public void testGetUserInfo_successSwitchUser() throws Exception {
        int userIdToSwitch = 42;
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(INITIAL_USER_INFO,
                    REQUEST_ID_PLACE_HOLDER, InitialUserInfoResponseAction.SWITCH);
        propResponse.value.int32Values.add(userIdToSwitch);

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                INITIAL_USER_INFO, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertInitialUserInfoSetRequest(reqCaptor.get(), COLD_BOOT);

        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        InitialUserInfoResponse actualResponse = callback.response;
        assertThat(actualResponse.action).isEqualTo(InitialUserInfoResponseAction.SWITCH);
        assertThat(actualResponse.userNameToCreate).isEmpty();
        UserInfo userToSwitch = actualResponse.userToSwitchOrCreate;
        assertThat(userToSwitch).isNotNull();
        assertThat(userToSwitch.userId).isEqualTo(userIdToSwitch);
        assertThat(userToSwitch.flags).isEqualTo(UserFlags.NONE);
    }

    @Test
    public void testGetUserInfo_successCreateUser() throws Exception {
        int newUserFlags = 108;
        String newUserName = "Groot";
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(INITIAL_USER_INFO,
                    REQUEST_ID_PLACE_HOLDER, InitialUserInfoResponseAction.CREATE);
        propResponse.value.int32Values.add(666); // userId (not used)
        propResponse.value.int32Values.add(newUserFlags);
        propResponse.value.stringValue = "||" + newUserName;

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                INITIAL_USER_INFO, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<InitialUserInfoResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.getInitialUserInfo(COLD_BOOT, TIMEOUT_MS, mUsersInfo,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertInitialUserInfoSetRequest(reqCaptor.get(), COLD_BOOT);

        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        assertThat(callback.status).isEqualTo(HalCallback.STATUS_OK);
        InitialUserInfoResponse actualResponse = callback.response;
        assertThat(actualResponse.action).isEqualTo(InitialUserInfoResponseAction.CREATE);
        assertThat(actualResponse.userLocales).isEmpty();
        assertThat(actualResponse.userNameToCreate).isEqualTo(newUserName);
        UserInfo newUser = actualResponse.userToSwitchOrCreate;
        assertThat(newUser).isNotNull();
        assertThat(newUser.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(newUser.flags).isEqualTo(newUserFlags);
    }

    @Test
    public void testGetUserInfo_twoSuccessfulCalls() throws Exception {
        testGetUserInfo_successDefault();
        testGetUserInfo_successDefault();
    }

    @Test
    public void testSwitchUser_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class,
                () -> myHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo),
                        TIMEOUT_MS, noOpCallback()));
    }

    @Test
    public void testSwitchUser_invalidTimeout() {
        assertThrows(IllegalArgumentException.class, () -> mUserHalService
                .switchUser(createUserSwitchRequest(mUser10, mUsersInfo), 0, noOpCallback()));
        assertThrows(IllegalArgumentException.class, () -> mUserHalService
                .switchUser(createUserSwitchRequest(mUser10, mUsersInfo), -1, noOpCallback()));
    }

    @Test
    public void testSwitchUser_noUsersInfo() {
        assertThrows(IllegalArgumentException.class, () -> mUserHalService
                .switchUser(createUserSwitchRequest(mUser10, null), TIMEOUT_MS, noOpCallback()));
    }

    @Test
    public void testSwitchUser_noCallback() {
        assertThrows(NullPointerException.class, () -> mUserHalService
                .switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS, null));
    }

    @Test
    public void testSwitchUser_nullRequest() {
        assertThrows(NullPointerException.class, () -> mUserHalService
                .switchUser(null, TIMEOUT_MS, noOpCallback()));
    }

    @Test
    public void testSwitchUser_noTarget() {
        assertThrows(NullPointerException.class, () -> mUserHalService
                .switchUser(createUserSwitchRequest(null, mUsersInfo), TIMEOUT_MS, noOpCallback()));
    }

    @Test
    public void testSwitchUser_halSetTimedOut() throws Exception {
        replySetPropertyWithTimeoutException(SWITCH_USER);

        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_SET_TIMEOUT);
        assertThat(callback.response).isNull();

        // Make sure the pending request was removed
        SystemClock.sleep(CALLBACK_TIMEOUT_TIMEOUT);
        callback.assertNotCalledAgain();
    }

    @Test
    public void testSwitchUser_halDidNotReply() throws Exception {
        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSwitchUser_halReplyWithWrongRequestId() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                    REQUEST_ID_PLACE_HOLDER, InitialUserInfoResponseAction.SWITCH);

        replySetPropertyWithOnChangeEvent(SWITCH_USER, propResponse,
                /* rightRequestId= */ false);

        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSwitchUser_halReturnedInvalidMessageType() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                REQUEST_ID_PLACE_HOLDER, SwitchUserMessageType.LEGACY_ANDROID_SWITCH);
        propResponse.value.int32Values.add(SwitchUserStatus.SUCCESS);

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                SWITCH_USER, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertHalSetSwitchUserRequest(reqCaptor.get(), SwitchUserMessageType.ANDROID_SWITCH,
                mUser10);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSwitchUser_success() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                    REQUEST_ID_PLACE_HOLDER, SwitchUserMessageType.VEHICLE_RESPONSE);
        propResponse.value.int32Values.add(SwitchUserStatus.SUCCESS);

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                SWITCH_USER, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertHalSetSwitchUserRequest(reqCaptor.get(), SwitchUserMessageType.ANDROID_SWITCH,
                mUser10);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        SwitchUserResponse actualResponse = callback.response;
        assertThat(actualResponse.status).isEqualTo(SwitchUserStatus.SUCCESS);
        assertThat(actualResponse.messageType).isEqualTo(SwitchUserMessageType.VEHICLE_RESPONSE);
        assertThat(actualResponse.errorMessage).isEmpty();
    }

    @Test
    public void testSwitchUser_failure() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                    REQUEST_ID_PLACE_HOLDER, SwitchUserMessageType.VEHICLE_RESPONSE);
        propResponse.value.int32Values.add(SwitchUserStatus.FAILURE);
        propResponse.value.stringValue = "D'OH!";

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                SWITCH_USER, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertHalSetSwitchUserRequest(reqCaptor.get(), SwitchUserMessageType.ANDROID_SWITCH,
                mUser10);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        SwitchUserResponse actualResponse = callback.response;
        assertThat(actualResponse.status).isEqualTo(SwitchUserStatus.FAILURE);
        assertThat(actualResponse.messageType).isEqualTo(SwitchUserMessageType.VEHICLE_RESPONSE);
        assertThat(actualResponse.errorMessage).isEqualTo("D'OH!");
    }

    @Test
    public void testSwitchUser_secondCallFailWhilePending() throws Exception {
        GenericHalCallback<SwitchUserResponse> callback1 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        GenericHalCallback<SwitchUserResponse> callback2 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback1);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback2);

        callback1.assertCalled();
        assertCallbackStatus(callback1, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback1.response).isNull();

        callback2.assertCalled();
        assertCallbackStatus(callback2, HalCallback.STATUS_CONCURRENT_OPERATION);
        assertThat(callback1.response).isNull();
    }

    @Test
    public void testSwitchUser_halReturnedInvalidStatus() throws Exception {
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                    REQUEST_ID_PLACE_HOLDER, SwitchUserMessageType.VEHICLE_RESPONSE);
        propResponse.value.int32Values.add(/*status =*/ 110); // an invalid status

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                SWITCH_USER, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<SwitchUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.switchUser(createUserSwitchRequest(mUser10, mUsersInfo), TIMEOUT_MS,
                callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertHalSetSwitchUserRequest(reqCaptor.get(), SwitchUserMessageType.ANDROID_SWITCH,
                mUser10);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testUserSwitch_OEMRequest_success() throws Exception {
        int requestId = -4;
        int targetUserId = 11;
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                requestId, SwitchUserMessageType.VEHICLE_REQUEST);

        propResponse.value.int32Values.add(targetUserId);

        mUserHalService.onHalEvents(Arrays.asList(propResponse));
        waitForHandler();

        verify(mCarUserService).switchAndroidUserFromHal(requestId, targetUserId);
    }

    @Test
    public void testUserSwitch_OEMRequest_failure_positiveRequestId() throws Exception {
        int requestId = 4;
        int targetUserId = 11;
        VehiclePropValue propResponse = UserHalHelper.createPropRequest(SWITCH_USER,
                requestId, SwitchUserMessageType.VEHICLE_REQUEST);
        propResponse.value.int32Values.add(targetUserId);

        mUserHalService.onHalEvents(Arrays.asList(propResponse));
        waitForHandler();

        verify(mCarUserService, never()).switchAndroidUserFromHal(anyInt(), anyInt());
    }

    @Test
    public void testPostSwitchResponse_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class,
                () -> myHalService.postSwitchResponse(new SwitchUserRequest()));
    }

    @Test
    public void testPostSwitchResponse_nullRequest() {
        assertThrows(NullPointerException.class, () -> mUserHalService.postSwitchResponse(null));
    }

    @Test
    public void testPostSwitchResponse_noUsersInfo() {
        SwitchUserRequest request = createUserSwitchRequest(mUser10, null);
        request.requestId = 42;
        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.postSwitchResponse(request));
    }

    @Test
    public void testPostSwitchResponse_HalCalledWithCorrectProp() {
        SwitchUserRequest request = createUserSwitchRequest(mUser10, mUsersInfo);
        request.requestId = 42;
        mUserHalService.postSwitchResponse(request);
        ArgumentCaptor<VehiclePropValue> propCaptor =
                ArgumentCaptor.forClass(VehiclePropValue.class);
        verify(mVehicleHal).set(propCaptor.capture());
        VehiclePropValue prop = propCaptor.getValue();
        assertHalSetSwitchUserRequest(prop, SwitchUserMessageType.ANDROID_POST_SWITCH, mUser10);
    }

    @Test
    public void testLegacyUserSwitch_nullRequest() {
        assertThrows(NullPointerException.class, () -> mUserHalService.legacyUserSwitch(null));
    }

    @Test
    public void testLegacyUserSwitch_noMessageType() {
        SwitchUserRequest request = new SwitchUserRequest();

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.legacyUserSwitch(request));
    }

    @Test
    public void testLegacyUserSwitch_noTargetUserInfo() {
        SwitchUserRequest request = new SwitchUserRequest();
        request.messageType = SwitchUserMessageType.ANDROID_SWITCH;

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.legacyUserSwitch(request));
    }

    @Test
    public void testRemoveUser_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class,
                () -> myHalService.removeUser(new RemoveUserRequest()));
    }

    @Test
    public void testRemoveUser_nullRequest() {
        RemoveUserRequest request = null;

        assertThrows(NullPointerException.class,
                () -> mUserHalService.removeUser(request));
    }

    @Test
    public void testRemoveUser_noRequestId() {
        RemoveUserRequest request = new RemoveUserRequest();

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.removeUser(request));
    }

    @Test
    public void testRemoveUser_noRemovedUserInfo() {
        RemoveUserRequest request = new RemoveUserRequest();
        request.requestId = 1;

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.removeUser(request));
    }

    @Test
    public void testRemoveUser_noUsersInfo() {
        RemoveUserRequest request = new RemoveUserRequest();
        request.requestId = 1;
        request.removedUserInfo = mUser10;

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.removeUser(request));
    }

    @Test
    public void testRemoveUser_HalCalledWithCorrectProp() {
        RemoveUserRequest request = new RemoveUserRequest();
        request.removedUserInfo = mUser10;
        request.usersInfo = mUsersInfo;
        ArgumentCaptor<VehiclePropValue> propCaptor =
                ArgumentCaptor.forClass(VehiclePropValue.class);

        mUserHalService.removeUser(request);

        verify(mVehicleHal).set(propCaptor.capture());
        assertHalSetRemoveUserRequest(propCaptor.getValue(), mUser10);
    }

    @Test
    public void testLegacyUserSwitch_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class,
                () -> myHalService.legacyUserSwitch(new SwitchUserRequest()));
    }

    @Test
    public void testLegacyUserSwitch_noUsersInfo() {
        SwitchUserRequest request = new SwitchUserRequest();
        request.messageType = SwitchUserMessageType.ANDROID_SWITCH;
        request.targetUser = mUser10;

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.legacyUserSwitch(request));
    }

    @Test
    public void testLegacyUserSwitch_HalCalledWithCorrectProp() {
        SwitchUserRequest request = new SwitchUserRequest();
        request.messageType = SwitchUserMessageType.LEGACY_ANDROID_SWITCH;
        request.targetUser = mUser10;
        request.usersInfo = mUsersInfo;

        mUserHalService.legacyUserSwitch(request);
        ArgumentCaptor<VehiclePropValue> propCaptor =
                ArgumentCaptor.forClass(VehiclePropValue.class);
        verify(mVehicleHal).set(propCaptor.capture());
        VehiclePropValue prop = propCaptor.getValue();
        assertHalSetSwitchUserRequest(prop, SwitchUserMessageType.LEGACY_ANDROID_SWITCH,
                mUser10);
    }

    @Test
    public void testCreateUser_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class,
                () -> myHalService.createUser(new CreateUserRequest(), TIMEOUT_MS, noOpCallback()));
    }

    @Test
    public void testCreateUser_noRequest() {
        assertThrows(NullPointerException.class, () -> mUserHalService
                .createUser(null, TIMEOUT_MS, noOpCallback()));
    }

    @Test
    public void testCreateUser_invalidTimeout() {
        assertThrows(IllegalArgumentException.class, () -> mUserHalService
                .createUser(new CreateUserRequest(), 0, noOpCallback()));
        assertThrows(IllegalArgumentException.class, () -> mUserHalService
                .createUser(new CreateUserRequest(), -1, noOpCallback()));
    }

    @Test
    public void testCreateUser_noCallback() {
        CreateUserRequest request = new CreateUserRequest();
        request.newUserInfo.userId = 10;
        request.usersInfo.existingUsers.add(request.newUserInfo);

        assertThrows(NullPointerException.class, () -> mUserHalService
                .createUser(request, TIMEOUT_MS, null));
    }

    /**
     * Creates a valid {@link CreateUserRequest} for tests that doesn't check its contents.
     */
    @NonNull
    private CreateUserRequest newValidCreateUserRequest() {
        CreateUserRequest request = new CreateUserRequest();
        request.newUserInfo = mUser10;
        request.usersInfo = mUsersInfo;
        return request;
    }

    @Test
    public void testCreateUser_halSetTimedOut() throws Exception {
        replySetPropertyWithTimeoutException(CREATE_USER);

        GenericHalCallback<CreateUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.createUser(newValidCreateUserRequest(), TIMEOUT_MS, callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_SET_TIMEOUT);
        assertThat(callback.response).isNull();

        // Make sure the pending request was removed
        SystemClock.sleep(CALLBACK_TIMEOUT_TIMEOUT);
        callback.assertNotCalledAgain();
    }

    @Test
    public void testCreateUser_halDidNotReply() throws Exception {
        GenericHalCallback<CreateUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.createUser(newValidCreateUserRequest(), TIMEOUT_MS, callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testCreateUser_halReplyWithWrongRequestId() throws Exception {
        VehiclePropValue propResponse =
                UserHalHelper.createPropRequest(CREATE_USER, REQUEST_ID_PLACE_HOLDER);

        replySetPropertyWithOnChangeEvent(CREATE_USER, propResponse,
                /* rightRequestId= */ false);

        GenericHalCallback<CreateUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.createUser(newValidCreateUserRequest(), TIMEOUT_MS, callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testCreateUser_success() throws Exception {
        VehiclePropValue propResponse =
                UserHalHelper.createPropRequest(CREATE_USER, REQUEST_ID_PLACE_HOLDER);
        propResponse.value.int32Values.add(CreateUserStatus.SUCCESS);

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                CREATE_USER, propResponse, /* rightRequestId= */ true);

        CreateUserRequest request = new CreateUserRequest();
        request.newUserInfo = mUser10;
        request.usersInfo = mUsersInfo;
        GenericHalCallback<CreateUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.createUser(request, TIMEOUT_MS, callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertHalSetCreateUserRequest(reqCaptor.get(), request);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        CreateUserResponse actualResponse = callback.response;
        assertThat(actualResponse.status).isEqualTo(CreateUserStatus.SUCCESS);
        assertThat(actualResponse.errorMessage).isEmpty();
    }

    @Test
    public void testCreateUser_failure() throws Exception {
        VehiclePropValue propResponse =
                UserHalHelper.createPropRequest(CREATE_USER, REQUEST_ID_PLACE_HOLDER);
        propResponse.value.int32Values.add(CreateUserStatus.FAILURE);
        propResponse.value.stringValue = "D'OH!";

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                CREATE_USER, propResponse, /* rightRequestId= */ true);

        CreateUserRequest request = new CreateUserRequest();
        request.newUserInfo = mUser10;
        request.usersInfo = mUsersInfo;
        GenericHalCallback<CreateUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.createUser(request, TIMEOUT_MS, callback);

        callback.assertCalled();

        // Make sure the arguments were properly converted
        assertHalSetCreateUserRequest(reqCaptor.get(), request);

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_OK);
        CreateUserResponse actualResponse = callback.response;
        assertThat(actualResponse.status).isEqualTo(CreateUserStatus.FAILURE);
        assertThat(actualResponse.errorMessage).isEqualTo("D'OH!");
    }

    @Test
    public void testCreateUser_secondCallFailWhilePending() throws Exception {
        GenericHalCallback<CreateUserResponse> callback1 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        GenericHalCallback<CreateUserResponse> callback2 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        mUserHalService.createUser(newValidCreateUserRequest(), TIMEOUT_MS, callback1);
        mUserHalService.createUser(newValidCreateUserRequest(), TIMEOUT_MS, callback2);

        callback1.assertCalled();
        assertCallbackStatus(callback1, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback1.response).isNull();

        callback2.assertCalled();
        assertCallbackStatus(callback2, HalCallback.STATUS_CONCURRENT_OPERATION);
        assertThat(callback1.response).isNull();
    }

    @Test
    public void testCreateUser_halReturnedInvalidStatus() throws Exception {
        VehiclePropValue propResponse =
                UserHalHelper.createPropRequest(CREATE_USER, REQUEST_ID_PLACE_HOLDER);
        propResponse.value.int32Values.add(/*status =*/ -1); // an invalid status

        AtomicReference<VehiclePropValue> reqCaptor = replySetPropertyWithOnChangeEvent(
                CREATE_USER, propResponse, /* rightRequestId= */ true);

        GenericHalCallback<CreateUserResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_SUCCESS);
        mUserHalService.createUser(newValidCreateUserRequest(), TIMEOUT_MS, callback);

        callback.assertCalled();

        // Assert response
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testGetUserAssociation_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class,
                () -> myHalService.getUserAssociation(new UserIdentificationGetRequest()));
    }

    @Test
    public void testGetUserAssociation_nullRequest() {
        assertThrows(NullPointerException.class, () -> mUserHalService.getUserAssociation(null));
    }

    @Test
    public void testGetUserAssociation_requestWithDuplicatedTypes() {
        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        request.numberAssociationTypes = 2;
        request.associationTypes.add(KEY_FOB);
        request.associationTypes.add(KEY_FOB);

        assertThrows(IllegalArgumentException.class,
                () -> mUserHalService.getUserAssociation(request));
    }

    @Test
    public void testGetUserAssociation_invalidResponse() {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(1); // 1 associations
        propResponse.value.int32Values.add(KEY_FOB); // type only, it's missing value
        UserIdentificationGetRequest request = replyToValidGetUserIdentificationRequest(
                propResponse);

        assertThat(mUserHalService.getUserAssociation(request)).isNull();
    }

    @Test
    public void testGetUserAssociation_nullResponse() {
        UserIdentificationGetRequest request = replyToValidGetUserIdentificationRequest(null);

        assertThat(mUserHalService.getUserAssociation(request)).isNull();

        verifyValidGetUserIdentificationRequestMade();
    }

    @Test
    public void testGetUserAssociation_wrongNumberOfAssociationsOnResponse() {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(2); // 2 associations
        propResponse.value.int32Values.add(KEY_FOB);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        propResponse.value.int32Values.add(CUSTOM_1);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        UserIdentificationGetRequest request = replyToValidGetUserIdentificationRequest(
                propResponse);

        assertThat(mUserHalService.getUserAssociation(request)).isNull();

        verifyValidGetUserIdentificationRequestMade();
    }

    @Test
    public void testGetUserAssociation_typesOnResponseMismatchTypesOnRequest() {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(1); // 1 association
        propResponse.value.int32Values.add(CUSTOM_1);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        UserIdentificationGetRequest request = replyToValidGetUserIdentificationRequest(
                propResponse);

        assertThat(mUserHalService.getUserAssociation(request)).isNull();

        verifyValidGetUserIdentificationRequestMade();
    }

    @Test
    public void testGetUserAssociation_requestIdMismatch() {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID + 1);
        propResponse.value.int32Values.add(1); // 1 association
        propResponse.value.int32Values.add(KEY_FOB);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        UserIdentificationGetRequest request = replyToValidGetUserIdentificationRequest(
                propResponse);

        assertThat(mUserHalService.getUserAssociation(request)).isNull();

        verifyValidGetUserIdentificationRequestMade();
    }

    @Test
    public void testGetUserAssociation_ok() {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(1); // 1 association
        propResponse.value.int32Values.add(KEY_FOB);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        UserIdentificationGetRequest request = replyToValidGetUserIdentificationRequest(
                propResponse);

        UserIdentificationResponse response = mUserHalService.getUserAssociation(request);

        assertThat(response.requestId).isEqualTo(DEFAULT_REQUEST_ID);
        assertThat(response.numberAssociation).isEqualTo(1);
        assertThat(response.associations).hasSize(1);
        UserIdentificationAssociation actualAssociation = response.associations.get(0);
        assertThat(actualAssociation.type).isEqualTo(KEY_FOB);
        assertThat(actualAssociation.value).isEqualTo(ASSOCIATED_CURRENT_USER);
    }

    @Test
    public void testSetUserAssociation_noHalSupported() {
        // Cannot use mUserHalService because it's already set with supported properties
        UserHalService myHalService = new UserHalService(mVehicleHal);

        assertThrows(IllegalStateException.class, () -> myHalService.setUserAssociation(TIMEOUT_MS,
                new UserIdentificationSetRequest(), noOpCallback()));
    }

    @Test
    public void testSetUserAssociation_invalidTimeout() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        assertThrows(IllegalArgumentException.class, () ->
                mUserHalService.setUserAssociation(0, request, noOpCallback()));
        assertThrows(IllegalArgumentException.class, () ->
                mUserHalService.setUserAssociation(-1, request, noOpCallback()));
    }

    @Test
    public void testSetUserAssociation_nullRequest() {
        assertThrows(NullPointerException.class, () ->
                mUserHalService.setUserAssociation(TIMEOUT_MS, null, noOpCallback()));
    }

    @Test
    public void testSetUserAssociation_nullCallback() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        assertThrows(NullPointerException.class, () ->
                mUserHalService.setUserAssociation(TIMEOUT_MS, request, null));
    }

    @Test
    public void testSetUserAssociation_requestWithDuplicatedTypes() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.numberAssociations = 2;
        UserIdentificationSetAssociation association1 = new UserIdentificationSetAssociation();
        association1.type = KEY_FOB;
        association1.value = ASSOCIATE_CURRENT_USER;
        request.associations.add(association1);
        request.associations.add(association1);

        assertThrows(IllegalArgumentException.class, () ->
                mUserHalService.setUserAssociation(TIMEOUT_MS, request, noOpCallback()));
    }

    @Test
    public void testSetUserAssociation_halSetTimedOut() throws Exception {
        UserIdentificationSetRequest request = validUserIdentificationSetRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        replySetPropertyWithTimeoutException(USER_IDENTIFICATION_ASSOCIATION);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_SET_TIMEOUT);
        assertThat(callback.response).isNull();

        // Make sure the pending request was removed
        SystemClock.sleep(CALLBACK_TIMEOUT_TIMEOUT);
        callback.assertNotCalledAgain();
    }

    @Test
    public void testSetUserAssociation_halDidNotReply() throws Exception {
        UserIdentificationSetRequest request = validUserIdentificationSetRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSetUserAssociation_secondCallFailWhilePending() throws Exception {
        UserIdentificationSetRequest request = validUserIdentificationSetRequest();
        GenericHalCallback<UserIdentificationResponse> callback1 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);
        GenericHalCallback<UserIdentificationResponse> callback2 = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback1);
        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback2);

        callback1.assertCalled();
        assertCallbackStatus(callback1, HalCallback.STATUS_HAL_RESPONSE_TIMEOUT);
        assertThat(callback1.response).isNull();

        callback2.assertCalled();
        assertCallbackStatus(callback2, HalCallback.STATUS_CONCURRENT_OPERATION);
        assertThat(callback1.response).isNull();
    }

    @Test
    public void testSetUserAssociation_responseWithWrongRequestId() throws Exception {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID + 1);
        AtomicReference<VehiclePropValue> propRequest = replySetPropertyWithOnChangeEvent(
                USER_IDENTIFICATION_ASSOCIATION, propResponse, /* rightRequestId= */ true);
        UserIdentificationSetRequest request = replyToValidSetUserIdentificationRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        // Assert request
        verifyValidSetUserIdentificationRequestMade(propRequest.get());
        // Assert response
        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSetUserAssociation_notEnoughValuesOnResponse() throws Exception {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        // need at least 4: requestId, number associations, type1, value1
        propResponse.value.int32Values.add(1);
        propResponse.value.int32Values.add(2);
        propResponse.value.int32Values.add(3);

        AtomicReference<VehiclePropValue> propRequest = replySetPropertyWithOnChangeEvent(
                USER_IDENTIFICATION_ASSOCIATION, propResponse, /* rightRequestId= */ true);
        UserIdentificationSetRequest request = replyToValidSetUserIdentificationRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        // Assert request
        verifyValidSetUserIdentificationRequestMade(propRequest.get());
        // Assert response
        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSetUserAssociation_wrongNumberOfAssociationsOnResponse() throws Exception {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(2); // 2 associations; request is just 1
        propResponse.value.int32Values.add(KEY_FOB);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        propResponse.value.int32Values.add(CUSTOM_1);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);

        AtomicReference<VehiclePropValue> propRequest = replySetPropertyWithOnChangeEvent(
                USER_IDENTIFICATION_ASSOCIATION, propResponse, /* rightRequestId= */ true);
        UserIdentificationSetRequest request = replyToValidSetUserIdentificationRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        // Assert request
        verifyValidSetUserIdentificationRequestMade(propRequest.get());
        // Assert response
        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSetUserAssociation_typeMismatchOnResponse() throws Exception {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(1); // 1 association
        propResponse.value.int32Values.add(CUSTOM_1); // request is KEY_FOB
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);

        AtomicReference<VehiclePropValue> propRequest = replySetPropertyWithOnChangeEvent(
                USER_IDENTIFICATION_ASSOCIATION, propResponse, /* rightRequestId= */ true);
        UserIdentificationSetRequest request = replyToValidSetUserIdentificationRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        // Assert request
        verifyValidSetUserIdentificationRequestMade(propRequest.get());
        // Assert response
        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_WRONG_HAL_RESPONSE);
        assertThat(callback.response).isNull();
    }

    @Test
    public void testSetUserAssociation_ok() throws Exception {
        VehiclePropValue propResponse = new VehiclePropValue();
        propResponse.prop = USER_IDENTIFICATION_ASSOCIATION;
        propResponse.value.int32Values.add(DEFAULT_REQUEST_ID);
        propResponse.value.int32Values.add(1); // 1 association
        propResponse.value.int32Values.add(KEY_FOB);
        propResponse.value.int32Values.add(ASSOCIATED_CURRENT_USER);

        AtomicReference<VehiclePropValue> propRequest = replySetPropertyWithOnChangeEvent(
                USER_IDENTIFICATION_ASSOCIATION, propResponse, /* rightRequestId= */ true);
        UserIdentificationSetRequest request = replyToValidSetUserIdentificationRequest();
        GenericHalCallback<UserIdentificationResponse> callback = new GenericHalCallback<>(
                CALLBACK_TIMEOUT_TIMEOUT);

        mUserHalService.setUserAssociation(TIMEOUT_MS, request, callback);

        // Assert request
        verifyValidSetUserIdentificationRequestMade(propRequest.get());
        // Assert response
        callback.assertCalled();
        assertCallbackStatus(callback, HalCallback.STATUS_OK);

        UserIdentificationResponse actualResponse = callback.response;

        assertThat(actualResponse.requestId).isEqualTo(DEFAULT_REQUEST_ID);
        assertThat(actualResponse.numberAssociation).isEqualTo(1);
        assertThat(actualResponse.associations).hasSize(1);
        UserIdentificationAssociation actualAssociation = actualResponse.associations.get(0);
        assertThat(actualAssociation.type).isEqualTo(KEY_FOB);
        assertThat(actualAssociation.value).isEqualTo(ASSOCIATED_CURRENT_USER);
    }

    /**
     * Asserts the given {@link UsersInfo} is properly represented in the {@link VehiclePropValue}.
     *
     * @param value property containing the info
     * @param info info to be checked
     * @param initialIndex first index of the info values in the property's {@code int32Values}
     */
    private void assertUsersInfo(VehiclePropValue value, UsersInfo info, int initialIndex) {
        // TODO: consider using UserHalHelper to convert the property into a specific request,
        // and compare the request's UsersInfo.
        // But such method is not needed in production code yet.
        ArrayList<Integer> values = value.value.int32Values;
        assertWithMessage("wrong values size").that(values)
                .hasSize(initialIndex + 3 + info.numberUsers * 2);

        int i = initialIndex;
        assertWithMessage("currentUser.id mismatch at index %s", i).that(values.get(i))
                .isEqualTo(info.currentUser.userId);
        i++;
        assertWithMessage("currentUser.flags mismatch at index %s", i).that(values.get(i))
            .isEqualTo(info.currentUser.flags);
        i++;
        assertWithMessage("numberUsers mismatch at index %s", i).that(values.get(i))
            .isEqualTo(info.numberUsers);
        i++;

        for (int j = 0; j < info.numberUsers; j++) {
            int actualUserId = values.get(i++);
            int actualUserFlags = values.get(i++);
            UserInfo expectedUser = info.existingUsers.get(j);
            assertWithMessage("wrong id for existing user#%s at index %s", j, i)
                .that(actualUserId).isEqualTo(expectedUser.userId);
            assertWithMessage("wrong flags for existing user#%s at index %s", j, i)
                .that(actualUserFlags).isEqualTo(expectedUser.flags);
        }
    }

    /**
     * Sets the VHAL mock to emulate a property change event upon a call to set a property.
     *
     * @param prop prop to be set
     * @param response response to be set on event
     * @param rightRequestId whether the response id should match the request
     * @return
     *
     * @return reference to the value passed to {@code set()}.
     */
    private AtomicReference<VehiclePropValue> replySetPropertyWithOnChangeEvent(int prop,
            VehiclePropValue response, boolean rightRequestId) throws Exception {
        AtomicReference<VehiclePropValue> ref = new AtomicReference<>();
        doAnswer((inv) -> {
            VehiclePropValue request = inv.getArgument(0);
            ref.set(request);
            int requestId = request.value.int32Values.get(0);
            int responseId = rightRequestId ? requestId : requestId + 1000;
            response.value.int32Values.set(0, responseId);
            Log.d(TAG, "replySetPropertyWithOnChangeEvent(): resp=" + response + " for req="
                    + request);
            mUserHalService.onHalEvents(Arrays.asList(response));
            return null;
        }).when(mVehicleHal).set(isProperty(prop));
        return ref;
    }

    /**
     * Sets the VHAL mock to emulate a property timeout exception upon a call to set a property.
     */
    private void replySetPropertyWithTimeoutException(int prop) throws Exception {
        doThrow(new ServiceSpecificException(VehicleHalStatusCode.STATUS_TRY_AGAIN,
                "PropId: 0x" + Integer.toHexString(prop))).when(mVehicleHal).set(isProperty(prop));
    }

    @NonNull
    private SwitchUserRequest createUserSwitchRequest(@NonNull UserInfo targetUser,
            @NonNull UsersInfo usersInfo) {
        SwitchUserRequest request = new SwitchUserRequest();
        request.targetUser = targetUser;
        request.usersInfo = usersInfo;
        return request;
    }

    /**
     * Creates and set expectations for a valid request.
     */
    private UserIdentificationGetRequest replyToValidGetUserIdentificationRequest(
            @NonNull VehiclePropValue response) {
        mockNextRequestId(DEFAULT_REQUEST_ID);

        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        request.userInfo.userId = DEFAULT_USER_ID;
        request.userInfo.flags = DEFAULT_USER_FLAGS;
        request.numberAssociationTypes = 1;
        request.associationTypes.add(KEY_FOB);

        when(mVehicleHal.get(isPropertyWithValues(USER_IDENTIFICATION_ASSOCIATION,
                DEFAULT_REQUEST_ID, DEFAULT_USER_ID, DEFAULT_USER_FLAGS,
                /* numberAssociations= */ 1, KEY_FOB)))
            .thenReturn(response);

        return request;
    }

    /**
     * Creates and set expectations for a valid request.
     */
    private UserIdentificationSetRequest replyToValidSetUserIdentificationRequest() {
        mockNextRequestId(DEFAULT_REQUEST_ID);
        return validUserIdentificationSetRequest();
    }

    /**
     * Creates a valid request that can be used in test cases where its content is not asserted.
     */
    private UserIdentificationSetRequest validUserIdentificationSetRequest() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.userInfo.userId = DEFAULT_USER_ID;
        request.userInfo.flags = DEFAULT_USER_FLAGS;
        request.numberAssociations = 1;
        UserIdentificationSetAssociation association1 = new UserIdentificationSetAssociation();
        association1.type = KEY_FOB;
        association1.value = ASSOCIATE_CURRENT_USER;
        request.associations.add(association1);
        return request;
    }

    /**
     * Run empty runnable to make sure that all posted handlers are done.
     */
    private void waitForHandler() {
        mHandler.runWithScissors(() -> { }, /* Default timeout */ CALLBACK_TIMEOUT_TIMEOUT);
    }

    private void mockNextRequestId(int requestId) {
        doReturn(requestId).when(mUserHalService).getNextRequestId();
    }

    private void assertInitialUserInfoSetRequest(VehiclePropValue req, int requestType) {
        assertThat(req.value.int32Values.get(1)).isEqualTo(requestType);
        assertUsersInfo(req, mUsersInfo, 2);
    }

    private void assertHalSetSwitchUserRequest(VehiclePropValue req, int messageType,
            UserInfo targetUserInfo) {
        assertThat(req.prop).isEqualTo(SWITCH_USER);
        assertWithMessage("wrong request Id on %s", req).that(req.value.int32Values.get(0))
            .isAtLeast(1);
        assertThat(req.value.int32Values.get(1)).isEqualTo(messageType);
        assertWithMessage("targetuser.id mismatch on %s", req).that(req.value.int32Values.get(2))
                .isEqualTo(targetUserInfo.userId);
        assertWithMessage("targetuser.flags mismatch on %s", req).that(req.value.int32Values.get(3))
                .isEqualTo(targetUserInfo.flags);
        assertUsersInfo(req, mUsersInfo, 4);
    }

    private void assertHalSetRemoveUserRequest(VehiclePropValue req, UserInfo userInfo) {
        assertThat(req.prop).isEqualTo(REMOVE_USER);
        assertWithMessage("wrong request Id on %s", req).that(req.value.int32Values.get(0))
                .isAtLeast(1);
        assertWithMessage("user.id mismatch on %s", req).that(req.value.int32Values.get(1))
                .isEqualTo(userInfo.userId);
        assertWithMessage("user.flags mismatch on %s", req).that(req.value.int32Values.get(2))
                .isEqualTo(userInfo.flags);
        assertUsersInfo(req, mUsersInfo, 3);
    }

    private void assertHalSetCreateUserRequest(VehiclePropValue prop, CreateUserRequest request) {
        assertThat(prop.prop).isEqualTo(CREATE_USER);
        assertWithMessage("wrong request Id on %s", prop).that(prop.value.int32Values.get(0))
                .isEqualTo(request.requestId);
        assertWithMessage("newUser.userId mismatch on %s", prop).that(prop.value.int32Values.get(1))
                .isEqualTo(request.newUserInfo.userId);
        assertWithMessage("newUser.flags mismatch on %s", prop).that(prop.value.int32Values.get(2))
                .isEqualTo(request.newUserInfo.flags);
        assertUsersInfo(prop, request.usersInfo, 3);
    }

    private void assertCallbackStatus(GenericHalCallback<?> callback, int expectedStatus) {
        int actualStatus = callback.status;
        if (actualStatus == expectedStatus) return;

        fail("Wrong callback status; expected "
                + UserHalHelper.halCallbackStatusToString(expectedStatus) + ", got "
                + UserHalHelper.halCallbackStatusToString(actualStatus));
    }

    /**
     * Verifies {@code hal.get()} was called with the values used on
     * {@link #replyToValidGetUserIdentificationRequest(VehiclePropValue)}.
     */
    private void verifyValidGetUserIdentificationRequestMade() {
        verify(mVehicleHal).get(isPropertyWithValues(USER_IDENTIFICATION_ASSOCIATION,
                DEFAULT_REQUEST_ID, DEFAULT_USER_ID, DEFAULT_USER_FLAGS,
                /* numberAssociations= */ 1, KEY_FOB));
    }

    /**
     * Verifies {@code hal.set()} was called with the values used on
     * {@link #replyToValidSetUserIdentificationRequest(VehiclePropValue)}.
     */
    private void verifyValidSetUserIdentificationRequestMade(@NonNull VehiclePropValue request) {
        assertThat(request.prop).isEqualTo(USER_IDENTIFICATION_ASSOCIATION);
        assertThat(request.value.int32Values).containsExactly(DEFAULT_REQUEST_ID, DEFAULT_USER_ID,
                DEFAULT_USER_FLAGS,
                /* numberAssociations= */ 1, KEY_FOB, ASSOCIATE_CURRENT_USER);
    }

    private static <T> HalCallback<T> noOpCallback() {
        return (i, r) -> { };
    }

    private final class GenericHalCallback<R> implements HalCallback<R> {

        private final CountDownLatch mLatch = new CountDownLatch(1);
        private final int mTimeout;
        private final List<Pair<Integer, R>> mExtraCalls = new ArrayList<>();

        public int status;
        public R response;

        GenericHalCallback(int timeout) {
            this.mTimeout = timeout;
        }

        @Override
        public void onResponse(int status, R response) {
            Log.d(TAG, "onResponse(): status=" + status + ", response=" +  response);
            this.status = status;
            this.response = response;
            if (mLatch.getCount() == 0) {
                Log.e(TAG, "Already responded");
                mExtraCalls.add(new Pair<>(status, response));
                return;
            }
            mLatch.countDown();
        }

        /**
         * Asserts that the callback was called, or fail if it timed out.
         */
        public void assertCalled() throws InterruptedException {
            Log.d(TAG, "assertCalled(): waiting " + mTimeout + "ms");
            if (!mLatch.await(mTimeout, TimeUnit.MILLISECONDS)) {
                throw new AssertionError("callback not called in " + mTimeout + "ms");
            }
        }

        /**
         * Asserts that the callback was not called more than once.
         */
        public void assertNotCalledAgain() {
            if (mExtraCalls.isEmpty()) return;
            throw new AssertionError("Called " + mExtraCalls.size() + " times more than expected: "
                    + mExtraCalls);
        }
    }
}