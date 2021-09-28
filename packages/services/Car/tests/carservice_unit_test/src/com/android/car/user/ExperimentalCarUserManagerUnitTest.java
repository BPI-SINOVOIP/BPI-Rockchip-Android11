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
package com.android.car.user;

import static android.car.test.mocks.AndroidMockitoHelper.getResult;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.when;

import android.annotation.UserIdInt;
import android.car.Car;
import android.car.ICarUserService;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.car.test.util.UserTestingHelper;
import android.car.user.CarUserManager;
import android.car.user.ExperimentalCarUserManager;
import android.car.user.UserCreationResult;
import android.car.user.UserSwitchResult;
import android.content.pm.UserInfo;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.internal.infra.AndroidFuture;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;

import java.util.Arrays;
import java.util.List;

public final class ExperimentalCarUserManagerUnitTest extends AbstractExtendedMockitoTestCase {

    @Mock
    private Car mCar;
    @Mock
    private UserManager mUserManager;
    @Mock
    private ICarUserService mService;

    private ExperimentalCarUserManager mManager;

    @Before public void setFixtures() {
        mManager =
                ExperimentalCarUserManager.from(new CarUserManager(mCar, mService, mUserManager));
    }

    @Test
    public void testCreateDriver_Success_Admin() throws Exception {
        String name = "test driver";
        int userId = 10;
        expectCreateDriverSucceed(name, userId);

        AndroidFuture<UserCreationResult> future = mManager.createDriver(name, true);

        UserCreationResult result = getResult(future);
        assertThat(result.getErrorMessage()).isNull();
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_SUCCESSFUL);
        assertThat(result.getUser().id).isEqualTo(userId);
    }

    @Test
    public void testCreateDriver_Success_NonAdmin() throws Exception {
        String name = "test driver";
        int userId = 10;
        expectCreateDriverSucceed(name, userId);

        AndroidFuture<UserCreationResult> future = mManager.createDriver(name, false);

        UserCreationResult result = getResult(future);
        assertThat(result.getErrorMessage()).isNull();
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_SUCCESSFUL);
        assertThat(result.getUser().id).isEqualTo(userId);
    }

    @Test
    public void testCreateDriver_Error() throws Exception {
        expectCreateDriverFail();

        AndroidFuture<UserCreationResult> future = mManager.createDriver("test driver", false);

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_HAL_INTERNAL_FAILURE);
    }

    @Test
    public void testCreatePassenger_Success() throws Exception {
        expectCreatePassengerSucceed();
        int userId = mManager.createPassenger("test passenger", 10);
        assertThat(userId).isNotEqualTo(UserHandle.USER_NULL);
    }

    @Test
    public void testCreatePassenger_Error() throws Exception {
        expectCreatePassengerFail();
        int userId = mManager.createPassenger("test passenger", 20);
        assertThat(userId).isEqualTo(UserHandle.USER_NULL);
    }

    @Test
    public void testSwitchDriver_Success() throws Exception {
        expectSwitchDriverSucceed(10);
        AndroidFuture<UserSwitchResult> future = mManager.switchDriver(10);
        UserSwitchResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_SUCCESSFUL);
    }

    @Test
    public void testSwitchDriver_Error() throws Exception {
        expectSwitchDriverFail(20);
        AndroidFuture<UserSwitchResult> future = mManager.switchDriver(20);
        assertThat(future).isNotNull();
        UserSwitchResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_HAL_INTERNAL_FAILURE);
    }

    @Test
    public void testGetAllDrivers() throws Exception {
        List<UserInfo> userInfos = UserTestingHelper.newUsers(10, 20, 30);
        when(mService.getAllDrivers()).thenReturn(userInfos);
        List<Integer> drivers = mManager.getAllDrivers();
        assertThat(drivers).containsExactly(10, 20, 30);
    }

    @Test
    public void testGetAllPassengers() throws Exception {
        List<UserInfo> userInfos = UserTestingHelper.newUsers(100, 101, 102);
        when(mService.getPassengers(10)).thenReturn(userInfos);
        when(mService.getPassengers(20)).thenReturn(Arrays.asList());

        List<Integer> passengers = mManager.getPassengers(10);
        assertThat(passengers).containsExactly(100, 101, 102);

        passengers = mManager.getPassengers(20);
        assertThat(passengers).isEmpty();
    }

    @Test
    public void testStartPassenger_Success() throws Exception {
        expectStartPassengerSucceed();
        boolean success = mManager.startPassenger(100, /* zoneId = */ 1);
        assertThat(success).isTrue();
    }

    @Test
    public void testStartPassenger_Error() throws Exception {
        expectStartPassengerFail();
        boolean success = mManager.startPassenger(200, /* zoneId = */ 1);
        assertThat(success).isFalse();
    }

    @Test
    public void testStopPassenger_Success() throws Exception {
        expectStopPassengerSucceed();
        boolean success = mManager.stopPassenger(100);
        assertThat(success).isTrue();
    }

    @Test
    public void testStopPassenger_Error() throws Exception {
        expectStopPassengerFail();
        boolean success = mManager.stopPassenger(200);
        assertThat(success).isFalse();
    }

    private void expectCreateDriverSucceed(String name, @UserIdInt int userId) throws Exception {
        AndroidFuture<UserCreationResult> future = new AndroidFuture<>();
        future.complete(new UserCreationResult(UserCreationResult.STATUS_SUCCESSFUL,
                UserTestingHelper.newUser(userId), null));
        when(mService.createDriver(eq(name), anyBoolean())).thenReturn(future);
    }

    private void expectCreateDriverFail() throws Exception {
        doThrow(new RemoteException("D'OH!")).when(mService)
            .createDriver(anyString(), anyBoolean());
    }

    private void expectCreatePassengerSucceed() throws Exception {
        UserInfo userInfo = UserTestingHelper.newUser(100);
        when(mService.createPassenger("test passenger", /* driverId = */ 10)).thenReturn(userInfo);
    }

    private void expectCreatePassengerFail() throws Exception {
        when(mService.createPassenger("test passenger", /* driverId = */ 10)).thenReturn(null);
    }

    private void expectSwitchDriverSucceed(@UserIdInt int userId) throws Exception {
        doAnswer((invocation) -> {
            @SuppressWarnings("unchecked")
            AndroidFuture<UserSwitchResult> future = (AndroidFuture<UserSwitchResult>) invocation
                    .getArguments()[1];
            future.complete(new UserSwitchResult(UserSwitchResult.STATUS_SUCCESSFUL, null));
            return null;
        }).when(mService).switchDriver(eq(userId), notNull());
    }

    private void expectSwitchDriverFail(@UserIdInt int userId) throws Exception {
        doThrow(new RemoteException("D'OH!")).when(mService)
            .switchDriver(eq(userId), notNull());
    }

    private void expectStartPassengerSucceed() throws Exception {
        when(mService.startPassenger(100, /* zoneId = */ 1)).thenReturn(true);
    }

    private void expectStartPassengerFail() throws Exception {
        when(mService.startPassenger(200, /* zoneId = */ 1)).thenReturn(false);
    }

    private void expectStopPassengerSucceed() throws Exception {
        when(mService.stopPassenger(100)).thenReturn(true);
    }

    private void expectStopPassengerFail() throws Exception {
        when(mService.stopPassenger(200)).thenReturn(false);
    }
}
