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
import static android.car.test.mocks.AndroidMockitoHelper.mockUmGetUsers;
import static android.car.testapi.CarMockitoHelper.mockHandleRemoteExceptionFromCarServiceWithDefaultValue;
import static android.os.UserHandle.USER_SYSTEM;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.car.Car;
import android.car.ICarUserService;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.car.test.util.UserTestingHelper;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.car.user.CarUserManager.UserSwitchUiCallback;
import android.car.user.UserCreationResult;
import android.car.user.UserIdentificationAssociationResponse;
import android.car.user.UserRemovalResult;
import android.car.user.UserSwitchResult;
import android.content.Context;
import android.content.pm.UserInfo;
import android.content.pm.UserInfo.UserInfoFlag;
import android.os.RemoteException;
import android.os.UserManager;
import android.provider.Settings;

import com.android.internal.infra.AndroidFuture;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;

public final class CarUserManagerUnitTest extends AbstractExtendedMockitoTestCase {

    @Mock
    private Car mCar;
    @Mock
    private UserManager mUserManager;
    @Mock
    private ICarUserService mService;
    @Mock
    private Context mMockContext;

    private CarUserManager mMgr;

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session.spyStatic(UserManager.class);
    }

    @Before
    public void setFixtures() {
        mMgr = new CarUserManager(mCar, mService, mUserManager);
        when(mCar.getContext()).thenReturn(mMockContext);
    }

    @Test
    public void testIsValidUser_headlessSystemUser() {
        mockIsHeadlessSystemUserMode(true);
        setExistingUsers(USER_SYSTEM);

        assertThat(mMgr.isValidUser(USER_SYSTEM)).isFalse();
    }

    @Test
    public void testIsValidUser_nonHeadlessSystemUser() {
        mockIsHeadlessSystemUserMode(false);
        setExistingUsers(USER_SYSTEM);

        assertThat(mMgr.isValidUser(USER_SYSTEM)).isTrue();
    }

    @Test
    public void testIsValidUser_found() {
        setExistingUsers(1, 2, 3);

        assertThat(mMgr.isValidUser(1)).isTrue();
        assertThat(mMgr.isValidUser(2)).isTrue();
        assertThat(mMgr.isValidUser(3)).isTrue();
    }

    @Test
    public void testIsValidUser_notFound() {
        setExistingUsers(1, 2, 3);

        assertThat(mMgr.isValidUser(4)).isFalse();
    }

    @Test
    public void testIsValidUser_emptyUsers() {
        assertThat(mMgr.isValidUser(666)).isFalse();
    }

    @Test
    public void testAddListener_nullExecutor() {
        assertThrows(NullPointerException.class, () -> mMgr.addListener(null, (e) -> { }));
    }

    @Test
    public void testAddListener_nullListener() {
        assertThrows(NullPointerException.class, () -> mMgr.addListener(Runnable::run, null));
    }

    @Test
    public void testAddListener_sameListenerAddedTwice() {
        UserLifecycleListener listener = (e) -> { };

        mMgr.addListener(Runnable::run, listener);
        assertThrows(IllegalStateException.class, () -> mMgr.addListener(Runnable::run, listener));
    }

    @Test
    public void testAddListener_differentListenersAddedTwice() {
        mMgr.addListener(Runnable::run, (e) -> { });
        mMgr.addListener(Runnable::run, (e) -> { });
    }

    @Test
    public void testRemoveListener_nullListener() {
        assertThrows(NullPointerException.class, () -> mMgr.removeListener(null));
    }

    @Test
    public void testRemoveListener_notAddedBefore() {
        UserLifecycleListener listener = (e) -> { };

        assertThrows(IllegalStateException.class, () -> mMgr.removeListener(listener));
    }

    @Test
    public void testRemoveListener_addAndRemove() {
        UserLifecycleListener listener = (e) -> { };

        mMgr.addListener(Runnable::run, listener);
        mMgr.removeListener(listener);

        // Make sure it was removed
        assertThrows(IllegalStateException.class, () -> mMgr.removeListener(listener));
    }

    @Test
    public void testSwitchUser_success() throws Exception {
        expectServiceSwitchUserSucceeds(11, UserSwitchResult.STATUS_SUCCESSFUL, "D'OH!");

        AndroidFuture<UserSwitchResult> future = mMgr.switchUser(11);

        assertThat(future).isNotNull();
        UserSwitchResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_SUCCESSFUL);
        assertThat(result.getErrorMessage()).isEqualTo("D'OH!");
    }

    @Test
    public void testSwitchUser_noUserSwitchability() throws Exception {
        when(mUserManager.getUserSwitchability())
                .thenReturn(UserManager.SWITCHABILITY_STATUS_SYSTEM_USER_LOCKED);

        AndroidFuture<UserSwitchResult> future = mMgr.switchUser(11);

        assertThat(future).isNotNull();
        UserSwitchResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_NOT_SWITCHABLE);
        assertThat(result.getErrorMessage()).isNull();
    }

    @Test
    public void testSwitchUser_remoteException() throws Exception {
        expectServiceSwitchUserFails(11);
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        AndroidFuture<UserSwitchResult> future = mMgr.switchUser(11);

        assertThat(future).isNotNull();
        UserSwitchResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_HAL_INTERNAL_FAILURE);
        assertThat(result.getErrorMessage()).isNull();
    }

    @Test
    public void testRemoveUser_success() throws Exception {
        int userId = 11;
        int status = UserRemovalResult.STATUS_SUCCESSFUL;
        when(mService.removeUser(userId)).thenReturn(new UserRemovalResult(status));

        UserRemovalResult result = mMgr.removeUser(11);

        assertThat(result.getStatus()).isEqualTo(UserRemovalResult.STATUS_SUCCESSFUL);
    }

    @Test
    public void testRemoveUser_remoteException() throws Exception {
        int userId = 11;
        doThrow(new RemoteException("D'OH!")).when(mService).removeUser(eq(userId));
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        UserRemovalResult result = mMgr.removeUser(11);

        assertThat(result.getStatus()).isEqualTo(UserRemovalResult.STATUS_HAL_INTERNAL_FAILURE);
    }

    @Test
    public void testSetSwitchUserUICallback_success() throws Exception {
        UserSwitchUiCallback callback = (u)-> { };

        mMgr.setUserSwitchUiCallback(callback);

        verify(mService).setUserSwitchUiCallback(any());
    }

    @Test
    public void testSetSwitchUserUICallback_nullCallback() throws Exception {
        assertThrows(IllegalArgumentException.class, () -> mMgr.setUserSwitchUiCallback(null));
    }

    @Test
    public void testCreateUser_withType_success() throws Exception {
        expectServiceCreateUserSucceeds("dude", "sweet", 42, UserCreationResult.STATUS_SUCCESSFUL,
                108);

        AndroidFuture<UserCreationResult> future = mMgr.createUser("dude", "sweet", 42);

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_SUCCESSFUL);
        assertThat(result.isSuccess()).isTrue();
        assertThat(result.getErrorMessage()).isNull();

        UserInfo newUser = result.getUser();
        assertThat(newUser).isNotNull();
        assertThat(newUser.id).isEqualTo(108);
        assertThat(newUser.name).isEqualTo("dude");
        assertThat(newUser.userType).isEqualTo("sweet");
        assertThat(newUser.flags).isEqualTo(42);
    }

    @Test
    public void testCreateUser_withType_remoteException() throws Exception {
        expectServiceCreateUserFails("dude", "sweet", 42);
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        AndroidFuture<UserCreationResult> future = mMgr.createUser("dude", "sweet", 42);

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_HAL_INTERNAL_FAILURE);
        assertThat(result.isSuccess()).isFalse();
        assertThat(result.getErrorMessage()).isNull();
        assertThat(result.getUser()).isNull();
    }

    @Test
    public void testCreateUser_success() throws Exception {
        expectServiceCreateUserSucceeds("dude", UserManager.USER_TYPE_FULL_SECONDARY, 42,
                UserCreationResult.STATUS_SUCCESSFUL, 108);

        AndroidFuture<UserCreationResult> future = mMgr.createUser("dude", 42);

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_SUCCESSFUL);
        assertThat(result.isSuccess()).isTrue();
        assertThat(result.getErrorMessage()).isNull();

        UserInfo newUser = result.getUser();
        assertThat(newUser).isNotNull();
        assertThat(newUser.id).isEqualTo(108);
        assertThat(newUser.name).isEqualTo("dude");
        assertThat(newUser.userType).isEqualTo(UserManager.USER_TYPE_FULL_SECONDARY);
        assertThat(newUser.flags).isEqualTo(42);
    }

    @Test
    public void testCreateUser_remoteException() throws Exception {
        expectServiceCreateUserFails("dude", UserManager.USER_TYPE_FULL_SECONDARY, 42);
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        AndroidFuture<UserCreationResult> future = mMgr.createUser("dude", 42);

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_HAL_INTERNAL_FAILURE);
        assertThat(result.isSuccess()).isFalse();
        assertThat(result.getErrorMessage()).isNull();
        assertThat(result.getUser()).isNull();
    }

    @Test
    public void testCreateGuest_success() throws Exception {
        expectServiceCreateUserSucceeds("dudeGuest", UserManager.USER_TYPE_FULL_GUEST, 0,
                UserCreationResult.STATUS_SUCCESSFUL, 108);

        AndroidFuture<UserCreationResult> future = mMgr.createGuest("dudeGuest");

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_SUCCESSFUL);
        assertThat(result.isSuccess()).isTrue();
        assertThat(result.getErrorMessage()).isNull();

        UserInfo newUser = result.getUser();
        assertThat(newUser).isNotNull();
        assertThat(newUser.id).isEqualTo(108);
        assertThat(newUser.name).isEqualTo("dudeGuest");
        assertThat(newUser.userType).isEqualTo(UserManager.USER_TYPE_FULL_GUEST);
        assertThat(newUser.flags).isEqualTo(0);
        assertThat(getSettingsString(Settings.Secure.SKIP_FIRST_USE_HINTS)).isEqualTo("1");
    }

    @Test
    public void testCreateGuest_remoteException() throws Exception {
        expectServiceCreateUserFails("dudeGuest", UserManager.USER_TYPE_FULL_GUEST, 0);
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        AndroidFuture<UserCreationResult> future = mMgr.createGuest("dudeGuest");

        assertThat(future).isNotNull();
        UserCreationResult result = getResult(future);
        assertThat(result.getStatus()).isEqualTo(UserCreationResult.STATUS_HAL_INTERNAL_FAILURE);
        assertThat(result.isSuccess()).isFalse();
        assertThat(result.getErrorMessage()).isNull();
        assertThat(result.getUser()).isNull();
    }

    @Test
    public void testGetUserIdentificationAssociation_nullTypes() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.getUserIdentificationAssociation(null));
    }

    @Test
    public void testGetUserIdentificationAssociation_emptyTypes() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.getUserIdentificationAssociation(new int[] {}));
    }

    @Test
    public void testGetUserIdentificationAssociation_remoteException() throws Exception {
        int[] types = new int[] {1};
        when(mService.getUserIdentificationAssociation(types))
                .thenThrow(new RemoteException("D'OH!"));
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        assertThat(mMgr.getUserIdentificationAssociation(types)).isNull();
    }

    @Test
    public void testGetUserIdentificationAssociation_ok() throws Exception {
        int[] types = new int[] { 4, 8, 15, 16, 23, 42 };
        UserIdentificationAssociationResponse expectedResponse =
                UserIdentificationAssociationResponse.forSuccess(types);
        when(mService.getUserIdentificationAssociation(types)).thenReturn(expectedResponse);

        UserIdentificationAssociationResponse actualResponse =
                mMgr.getUserIdentificationAssociation(types);

        assertThat(actualResponse).isSameAs(expectedResponse);
    }

    @Test
    public void testSetUserIdentificationAssociation_nullTypes() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.setUserIdentificationAssociation(null, new int[] {42}));
    }

    @Test
    public void testSetUserIdentificationAssociation_emptyTypes() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.setUserIdentificationAssociation(new int[0], new int[] {42}));
    }

    @Test
    public void testSetUserIdentificationAssociation_nullValues() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.setUserIdentificationAssociation(new int[] {42}, null));
    }

    @Test
    public void testSetUserIdentificationAssociation_emptyValues() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.setUserIdentificationAssociation(new int[] {42}, new int[0]));
    }

    @Test
    public void testSetUserIdentificationAssociation_sizeMismatch() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mMgr.setUserIdentificationAssociation(new int[] {1}, new int[] {2, 3}));
    }

    @Test
    public void testSetUserIdentificationAssociation_remoteException() throws Exception {
        int[] types = new int[] {1};
        int[] values = new int[] {2};
        doThrow(new RemoteException("D'OH!")).when(mService)
                .setUserIdentificationAssociation(anyInt(), same(types), same(values), notNull());
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        AndroidFuture<UserIdentificationAssociationResponse> future =
                mMgr.setUserIdentificationAssociation(types, values);

        assertThat(future).isNotNull();
        UserIdentificationAssociationResponse result = getResult(future);
        assertThat(result.isSuccess()).isFalse();
        assertThat(result.getValues()).isNull();
        assertThat(result.getErrorMessage()).isNull();
    }

    @Test
    public void testSetUserIdentificationAssociation_ok() throws Exception {
        int[] types = new int[] { 1, 2, 3 };
        int[] values = new int[] { 10, 20, 30 };
        doAnswer((inv) -> {
            @SuppressWarnings("unchecked")
            AndroidFuture<UserIdentificationAssociationResponse> future =
                    (AndroidFuture<UserIdentificationAssociationResponse>) inv.getArguments()[3];
            UserIdentificationAssociationResponse response =
                    UserIdentificationAssociationResponse.forSuccess(values, "D'OH!");
            future.complete(response);
            return null;
        }).when(mService)
                .setUserIdentificationAssociation(anyInt(), same(types), same(values), notNull());
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        AndroidFuture<UserIdentificationAssociationResponse> future =
                mMgr.setUserIdentificationAssociation(types, values);

        assertThat(future).isNotNull();
        UserIdentificationAssociationResponse result = getResult(future);
        assertThat(result.isSuccess()).isTrue();
        assertThat(result.getValues()).asList().containsAllOf(10, 20, 30).inOrder();
        assertThat(result.getErrorMessage()).isEqualTo("D'OH!");
    }

    @Test
    public void testIsUserHalUserAssociation() throws Exception {
        when(mService.isUserHalUserAssociationSupported()).thenReturn(false).thenReturn(true);

        assertThat(mMgr.isUserHalUserAssociationSupported()).isFalse();
        assertThat(mMgr.isUserHalUserAssociationSupported()).isTrue();
    }

    @Test
    public void testIsUserHalUserAssociation_remoteException() throws Exception {
        doThrow(new RemoteException("D'OH!")).when(mService).isUserHalUserAssociationSupported();
        mockHandleRemoteExceptionFromCarServiceWithDefaultValue(mCar);

        assertThat(mMgr.isUserHalUserAssociationSupported()).isFalse();
    }

    private void expectServiceSwitchUserSucceeds(@UserIdInt int userId,
            @UserSwitchResult.Status int status, @Nullable String errorMessage)
            throws RemoteException {
        doAnswer((invocation) -> {
            @SuppressWarnings("unchecked")
            AndroidFuture<UserSwitchResult> future = (AndroidFuture<UserSwitchResult>) invocation
                    .getArguments()[2];
            future.complete(new UserSwitchResult(status, errorMessage));
            return null;
        }).when(mService).switchUser(eq(userId), anyInt(), notNull());
    }

    private void expectServiceSwitchUserFails(@UserIdInt int userId) throws RemoteException {
        doThrow(new RemoteException("D'OH!")).when(mService)
            .switchUser(eq(userId), anyInt(), notNull());
    }

    private void expectServiceCreateUserSucceeds(@Nullable String name,
            @NonNull String userType, @UserInfoFlag int flags,
            @UserCreationResult.Status int status, @UserIdInt int userId) throws RemoteException {
        doAnswer((invocation) -> {
            @SuppressWarnings("unchecked")
            AndroidFuture<UserCreationResult> future =
                    (AndroidFuture<UserCreationResult>) invocation.getArguments()[4];
            UserInfo newUser = new UserTestingHelper.UserInfoBuilder(108)
                    .setName(name).setType(userType).setFlags(flags).build();
            future.complete(new UserCreationResult(status, newUser, /* errorMessage= */ null));
            return null;
        }).when(mService).createUser(eq(name), eq(userType), eq(flags), anyInt(), notNull());
    }

    private void expectServiceCreateUserFails(@Nullable String name,
            @NonNull String userType, @UserInfoFlag int flags) throws RemoteException {
        doThrow(new RemoteException("D'OH!")).when(mService)
                .createUser(eq(name), eq(userType), eq(flags), anyInt(), notNull());
    }

    private void setExistingUsers(int... userIds) {
        mockUmGetUsers(mUserManager, userIds);
    }
}
