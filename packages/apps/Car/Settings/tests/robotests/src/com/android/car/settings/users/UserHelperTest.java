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

package com.android.car.settings.users;

import static android.os.UserManager.USER_TYPE_SYSTEM_HEADLESS;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.car.user.CarUserManager;
import android.car.user.UserCreationResult;
import android.car.user.UserRemovalResult;
import android.car.user.UserSwitchResult;
import android.content.Context;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.car.settings.testutils.ShadowActivityManager;
import com.android.car.settings.testutils.ShadowUserIconProvider;
import com.android.car.settings.testutils.ShadowUserManager;
import com.android.internal.infra.AndroidFuture;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowProcess;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowActivityManager.class, ShadowUserManager.class,
        ShadowUserIconProvider.class})
public class UserHelperTest {

    private static final String DEFAULT_ADMIN_NAME = "default_admin";
    private static final String DEFAULT_GUEST_NAME = "default_guest";

    private Context mContext;
    private UserHelper mUserHelper;

    @Mock
    private UserManager mMockUserManager;
    @Mock
    private Resources mMockResources;
    @Mock
    private CarUserManager mMockCarUserManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mUserHelper = new UserHelper(mMockUserManager, mMockResources,
                DEFAULT_ADMIN_NAME, DEFAULT_GUEST_NAME, mMockCarUserManager);

        when(mMockUserManager.hasUserRestriction(UserManager.DISALLOW_MODIFY_ACCOUNTS))
                .thenReturn(false);
        when(mMockUserManager.isDemoUser()).thenReturn(false);
        when(mMockUserManager.isGuestUser()).thenReturn(false);

        ShadowUserManager.setIsHeadlessSystemUserMode(true);
        ShadowActivityManager.setCurrentUser(10);
    }

    @After
    public void tearDown() {
        ShadowActivityManager.reset();
        ShadowUserManager.reset();
    }

    @Test
    public void canCurrentProcessModifyAccounts_baseline_returnsTrue() {
        assertThat(mUserHelper.canCurrentProcessModifyAccounts()).isTrue();
    }

    @Test
    public void canCurrentProcessModifyAccounts_hasDisallowModifyAccounts_returnsFalse() {
        when(mMockUserManager.hasUserRestriction(UserManager.DISALLOW_MODIFY_ACCOUNTS))
                .thenReturn(true);
        assertThat(mUserHelper.canCurrentProcessModifyAccounts()).isFalse();
    }

    @Test
    public void canCurrentProcessModifyAccounts_isDemoUser_returnsFalse() {
        when(mMockUserManager.isDemoUser()).thenReturn(true);
        assertThat(mUserHelper.canCurrentProcessModifyAccounts()).isFalse();
    }

    @Test
    public void canCurrentProcessModifyAccounts_isGuestUser_returnsFalse() {
        when(mMockUserManager.isGuestUser()).thenReturn(true);
        assertThat(mUserHelper.canCurrentProcessModifyAccounts()).isFalse();
    }

    @Test
    public void testGetAllsers() {
        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two admin, and two non-admin users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createNonAdminUser(fgUserId);
        UserInfo user2 = createAdminUser(fgUserId + 1);
        UserInfo user3 = createNonAdminUser(fgUserId + 2);
        UserInfo user4 = createAdminUser(fgUserId + 3);

        mockGetUsers(systemUser, fgUser, user2, user3, user4);

        // Should return all non-system users
        assertThat(mUserHelper.getAllUsers()).containsExactly(fgUser, user2, user3, user4);
    }

    @Test
    public void testGetAllUsers_notHeadless() {
        ShadowUserManager.setIsHeadlessSystemUserMode(false);

        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two admin, and two non-admin users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createNonAdminUser(fgUserId);
        UserInfo user2 = createAdminUser(fgUserId + 1);
        UserInfo user3 = createNonAdminUser(fgUserId + 2);
        UserInfo user4 = createAdminUser(fgUserId + 3);

        mockGetUsers(systemUser, fgUser, user2, user3, user4);

        // Should return all users
        assertThat(mUserHelper.getAllUsers())
                .containsExactly(systemUser, fgUser, user2, user3, user4);
    }

    @Test
    public void testGetAllSwitchableUsers() {
        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two non-foreground users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createAdminUser(fgUserId);
        UserInfo user1 = createAdminUser(fgUserId + 1);
        UserInfo user2 = createAdminUser(fgUserId + 2);

        mockGetUsers(systemUser, fgUser, user1, user2);

        // Should return all non-foreground users.
        assertThat(mUserHelper.getAllSwitchableUsers()).containsExactly(user1, user2);
    }

    @Test
    public void testGetAllSwitchableUsers_notHeadless() {
        ShadowUserManager.setIsHeadlessSystemUserMode(false);

        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two non-foreground users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createAdminUser(fgUserId);
        UserInfo user1 = createAdminUser(fgUserId + 1);
        UserInfo user2 = createAdminUser(fgUserId + 2);

        mockGetUsers(systemUser, fgUser, user1, user2);

        // Should return all non-foreground users.
        assertThat(mUserHelper.getAllSwitchableUsers()).containsExactly(systemUser, user1, user2);
    }

    @Test
    public void testGetAllPersistentUsers() {
        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two non-ephemeral users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createAdminUser(fgUserId);
        UserInfo user2 = createAdminUser(fgUserId + 1);
        // Create two ephemeral users.
        UserInfo user3 = createEphemeralUser(fgUserId + 2);
        UserInfo user4 = createEphemeralUser(fgUserId + 3);

        mockGetUsers(systemUser, fgUser, user2, user3, user4);

        // Should return all non-ephemeral users.
        assertThat(mUserHelper.getAllPersistentUsers()).containsExactly(fgUser, user2);
    }

    @Test
    public void testGetAllPersistentUsers_notHeadless() {
        ShadowUserManager.setIsHeadlessSystemUserMode(false);

        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two non-ephemeral users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createAdminUser(fgUserId);
        UserInfo user2 = createAdminUser(fgUserId + 1);
        // Create two ephemeral users.
        UserInfo user3 = createEphemeralUser(fgUserId + 2);
        UserInfo user4 = createEphemeralUser(fgUserId + 3);

        mockGetUsers(systemUser, fgUser, user2, user3, user4);

        // Should return all non-ephemeral users.
        assertThat(mUserHelper.getAllPersistentUsers()).containsExactly(systemUser, fgUser, user2);
    }

    @Test
    public void testGetAllAdminUsers() {
        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two admin, and two non-admin users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createNonAdminUser(fgUserId);
        UserInfo user2 = createAdminUser(fgUserId + 1);
        UserInfo user3 = createNonAdminUser(fgUserId + 2);
        UserInfo user4 = createAdminUser(fgUserId + 3);

        mockGetUsers(systemUser, fgUser, user2, user3, user4);

        // Should return only admin users.
        assertThat(mUserHelper.getAllAdminUsers()).containsExactly(user2, user4);
    }

    @Test
    public void testGetAllAdminUsers_notHeadless() {
        ShadowUserManager.setIsHeadlessSystemUserMode(false);

        // Add system user
        UserInfo systemUser = createAdminUser(UserHandle.USER_SYSTEM);

        // Create two admin, and two non-admin users.
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createNonAdminUser(fgUserId);
        UserInfo user2 = createAdminUser(fgUserId + 1);
        UserInfo user3 = createNonAdminUser(fgUserId + 2);
        UserInfo user4 = createAdminUser(fgUserId + 3);

        mockGetUsers(systemUser, fgUser, user2, user3, user4);

        // Should return only admin users.
        assertThat(mUserHelper.getAllAdminUsers()).containsExactly(systemUser, user2, user4);
    }

    @Test
    public void testRemoveUser_isAdminUser_cannotRemoveSystemUser() {
        UserInfo systemUser = new UserInfo(
                UserHandle.USER_SYSTEM,
                "Driver",
                /* iconPath= */ null,
                /* flags= */ UserInfo.FLAG_ADMIN | UserInfo.FLAG_SYSTEM,
                /* userType= */ USER_TYPE_SYSTEM_HEADLESS);

        assertThat(mUserHelper.removeUser(mContext, systemUser)).isFalse();
    }

    @Test
    public void testRemoveUser_isAdmin_canRemoveOtherUsers() {
        // Create admin user and non-admin user
        int fgUserId = ActivityManager.getCurrentUser();
        int nonAdminUserId = fgUserId + 1;
        UserInfo fgUser = createAdminUser(fgUserId);
        UserInfo nonAdminUser = createNonAdminUser(nonAdminUserId);

        mockGetUsers(fgUser, nonAdminUser);
        mockRemoveUser(nonAdminUserId, UserRemovalResult.STATUS_SUCCESSFUL);
        // If Admin is removing non-current, non-system user, simply calls removeUser.
        when(mMockUserManager.isAdminUser()).thenReturn(true);

        assertThat(mUserHelper.removeUser(mContext, nonAdminUser)).isTrue();
        verify(mMockCarUserManager).removeUser(nonAdminUserId);
    }

    @Test
    public void testRemoveUser_isNonAdmin_cannotRemoveOtherUsers() {
        // Create two non-admin users
        int fgUserId = ActivityManager.getCurrentUser();
        UserInfo fgUser = createNonAdminUser(fgUserId);
        UserInfo user2 = createNonAdminUser(fgUserId + 1);
        mockGetUsers(fgUser, user2);

        // Make current user non-admin.
        when(mMockUserManager.isAdminUser()).thenReturn(false);
        // Mock so that removeUser always pretends it's successful.
        mockRemoveUserSuccess();

        // If Non-Admin is trying to remove someone other than themselves, they should fail.
        assertThat(mUserHelper.removeUser(mContext, user2)).isFalse();
        verify(mMockCarUserManager, never()).removeUser(user2.id);
    }


    @Test
    public void testRemoveUser_removesLastAdminUser_createsAndSwitchesToNewAdminUser() {
        // Ensure admin status
        when(mMockUserManager.isAdminUser()).thenReturn(true);

        // Create one admin and one non-admin
        int baseId = 10;
        UserInfo adminUser = createAdminUser(baseId);
        UserInfo nonAdminInfo = createNonAdminUser(baseId + 1);
        mockGetUsers(adminUser, nonAdminInfo);
        UserInfo newAdminInfo = createAdminUser(baseId + 2);
        mockCreateUser(DEFAULT_ADMIN_NAME, UserInfo.FLAG_ADMIN,
                UserCreationResult.STATUS_SUCCESSFUL, newAdminInfo);
        mockSwitchUserSuccess();

        mUserHelper.removeUser(mContext, adminUser);

        verify(mMockCarUserManager).createUser(DEFAULT_ADMIN_NAME, UserInfo.FLAG_ADMIN);
        verify(mMockCarUserManager).switchUser(newAdminInfo.id);
        verify(mMockCarUserManager).removeUser(adminUser.id);
    }

    @Test
    public void testRemoveUser_removesLastAdminUserFailsCreateNewUser_doesNotRemoveOrSwitchUser() {
        // Ensure admin status
        when(mMockUserManager.isAdminUser()).thenReturn(true);

        // Create one admin and one non-admin
        int baseId = 10;
        UserInfo adminUser = createAdminUser(baseId);
        UserInfo nonAdminInfo = createNonAdminUser(baseId + 1);
        mockGetUsers(adminUser, nonAdminInfo);

        // Fail to create a new user to force a failure case
        mockCreateUser(DEFAULT_ADMIN_NAME, UserInfo.FLAG_ADMIN,
                UserCreationResult.STATUS_ANDROID_FAILURE, null);

        mUserHelper.removeUser(mContext, adminUser);
        verify(mMockCarUserManager).createUser(DEFAULT_ADMIN_NAME, UserInfo.FLAG_ADMIN);
        verify(mMockCarUserManager, never()).switchUser(anyInt());
        verify(mMockCarUserManager, never()).removeUser(adminUser.id);
    }

    @Test
    public void testRemoveUser_removeForegroundUser_callsSwitchToGuest() {
        // Create foreground user
        int baseId = 10;
        ShadowProcess.setUid(baseId * UserHandle.PER_USER_RANGE); // User 10
        UserInfo currentUser = createNonAdminUser(baseId);
        when(mMockUserManager.isAdminUser()).thenReturn(false);
        mockGetUsers(currentUser);

        UserInfo guestUser = createGuestUser(baseId + 1);
        mockCreateGuest(DEFAULT_GUEST_NAME, UserCreationResult.STATUS_SUCCESSFUL, guestUser);
        mockSwitchUserSuccess();

        mUserHelper.removeUser(mContext, currentUser);
        verify(mMockCarUserManager).createGuest(DEFAULT_GUEST_NAME);
        verify(mMockCarUserManager).switchUser(guestUser.id);
        verify(mMockCarUserManager).removeUser(currentUser.id);
    }

    @Test
    public void testGetMaxSupportedRealUsers_isHeadless() {
        ShadowUserManager.setIsHeadlessSystemUserMode(true);
        ShadowUserManager.setMaxSupportedUsers(7);

        // Create System user, two managed profiles, and two normal users.
        UserInfo user0 = createAdminUser(0);
        UserInfo user1 = createNonAdminUser(10);
        UserInfo user2 = createManagedProfile(11);
        UserInfo user3 = createNonAdminUser(13);
        UserInfo user4 = createManagedProfile(14);

        mockGetUsers(user0, user1, user2, user3, user4);

        // Max users - # managed profiles - headless system user.
        assertThat(mUserHelper.getMaxSupportedRealUsers()).isEqualTo(4);
    }

    @Test
    public void testGetMaxSupportedRealUsers_isNotHeadless() {
        ShadowUserManager.setIsHeadlessSystemUserMode(false);
        ShadowUserManager.setMaxSupportedUsers(7);

        // Create System user, two managed profiles, and two normal users.
        UserInfo user0 = createAdminUser(0);
        UserInfo user1 = createNonAdminUser(10);
        UserInfo user2 = createManagedProfile(11);
        UserInfo user3 = createNonAdminUser(13);
        UserInfo user4 = createManagedProfile(14);

        mockGetUsers(user0, user1, user2, user3, user4);

        // Max users - # managed profiles
        assertThat(mUserHelper.getMaxSupportedRealUsers()).isEqualTo(5);
    }

    @Test
    public void testCreateNewOrFindExistingGuest_ifGuestExists_returnsExistingGuest() {
        // Create two users and a guest user.
        UserInfo user1 = createAdminUser(10);
        UserInfo user2 = createNonAdminUser(12);
        UserInfo guestUser = createGuestUser(13);

        mockGetUsers(user1, user2, guestUser);
        mockCreateUserFail();

        when(mMockUserManager.findCurrentGuestUser()).thenReturn(guestUser);

        UserInfo guest = mUserHelper.createNewOrFindExistingGuest(mContext);
        assertThat(guest).isEqualTo(guestUser);
    }

    @Test
    public void testCreateNewOrFindExistingGuest_ifNoGuest_createsNewGuest() {
        // Create two users.
        UserInfo user1 = createAdminUser(10);
        UserInfo user2 = createNonAdminUser(12);

        mockGetUsers(user1, user2);

        // Create a user for the "new guest" user.
        UserInfo guestInfo = createGuestUser(21);

        mockCreateGuest(DEFAULT_GUEST_NAME, UserCreationResult.STATUS_SUCCESSFUL, guestInfo);

        UserInfo guest = mUserHelper.createNewOrFindExistingGuest(mContext);
        verify(mMockCarUserManager).createGuest(DEFAULT_GUEST_NAME);
        assertThat(guest).isEqualTo(guestInfo);
    }

    private UserInfo createAdminUser(int id) {
        return new UserInfo(id, null, UserInfo.FLAG_ADMIN);
    }

    private UserInfo createNonAdminUser(int id) {
        return new UserInfo(id, null, 0);
    }

    private UserInfo createEphemeralUser(int id) {
        return new UserInfo(id, null, UserInfo.FLAG_EPHEMERAL);
    }

    private UserInfo createManagedProfile(int id) {
        return new UserInfo(id, null, UserInfo.FLAG_MANAGED_PROFILE);
    }

    private UserInfo createGuestUser(int id) {
        return new UserInfo(id, null, UserInfo.FLAG_GUEST);
    }

    private void mockGetUsers(UserInfo... users) {
        List<UserInfo> testUsers = new ArrayList<>(Arrays.asList(users));
        when(mMockUserManager.getUsers()).thenReturn(testUsers);
        when(mMockUserManager.getUsers(true)).thenReturn(testUsers);
        when(mMockUserManager.getUsers(false)).thenReturn(testUsers);
    }

    private void mockRemoveUser(int userId, int status) {
        when(mMockCarUserManager.removeUser(userId)).thenReturn(new UserRemovalResult(status));
    }

    private void mockRemoveUserSuccess() {
        when(mMockCarUserManager.removeUser(anyInt()))
                .thenReturn(new UserRemovalResult(UserRemovalResult.STATUS_SUCCESSFUL));
    }

    private void mockCreateUserFail() {
        AndroidFuture<UserCreationResult> future = new AndroidFuture<>();
        future.complete(new UserCreationResult(UserCreationResult.STATUS_ANDROID_FAILURE,
                null, null));
        when(mMockCarUserManager.createUser(any(), anyInt())).thenReturn(future);
        when(mMockCarUserManager.createGuest(any())).thenReturn(future);
    }

    private void mockCreateUser(String name, int flag, int status, UserInfo userInfo) {
        AndroidFuture<UserCreationResult> future = new AndroidFuture<>();
        future.complete(new UserCreationResult(status, userInfo, null));
        when(mMockCarUserManager.createUser(name, flag)).thenReturn(future);
    }

    private void mockCreateGuest(String name, int status, UserInfo userInfo) {
        AndroidFuture<UserCreationResult> future = new AndroidFuture<>();
        future.complete(new UserCreationResult(status, userInfo, null));
        when(mMockCarUserManager.createGuest(name)).thenReturn(future);
    }

    private void mockSwitchUserSuccess() {
        AndroidFuture<UserSwitchResult> future = new AndroidFuture<>();
        future.complete(new UserSwitchResult(UserSwitchResult.STATUS_SUCCESSFUL, null));
        when(mMockCarUserManager.switchUser(anyInt())).thenReturn(future);
    }
}
