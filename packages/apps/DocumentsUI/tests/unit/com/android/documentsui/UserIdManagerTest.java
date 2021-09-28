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

package com.android.documentsui;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.test.filters.SmallTest;

import com.android.documentsui.base.UserId;
import com.android.documentsui.util.VersionUtils;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;

@SmallTest
public class UserIdManagerTest {

    private final UserHandle systemUser = UserHandle.SYSTEM;
    private final UserHandle managedUser1 = UserHandle.of(100);
    private final UserHandle nonManagedUser1 = UserHandle.of(200);
    private final UserHandle nonManagedUser2 = UserHandle.of(201);

    private final Context mockContext = mock(Context.class);
    private final UserManager mockUserManager = mock(UserManager.class);

    private UserIdManager mUserIdManager;

    @Before
    public void setUp() throws Exception {
        when(mockContext.getApplicationContext()).thenReturn(mockContext);

        when(mockUserManager.isManagedProfile(managedUser1.getIdentifier())).thenReturn(true);
        when(mockUserManager.isManagedProfile(systemUser.getIdentifier())).thenReturn(false);
        when(mockUserManager.isManagedProfile(nonManagedUser1.getIdentifier())).thenReturn(false);
        when(mockUserManager.isManagedProfile(nonManagedUser2.getIdentifier())).thenReturn(false);

        when(mockContext.getSystemService(Context.USER_SERVICE)).thenReturn(mockUserManager);
    }

    // common cases for getUserIds
    @Test
    public void testGetUserIds_systemUser_currentUserIsSystemUser() {
        // Returns the current user if there is only system user.
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser));
        assertThat(mUserIdManager.getUserIds()).containsExactly(UserId.of(systemUser));
    }

    @Test
    public void testGetUserIds_systemUserAndManagedUser_currentUserIsSystemUser() {
        // Returns the both if there are system and managed users.
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser, managedUser1));
        assertThat(mUserIdManager.getUserIds())
                .containsExactly(UserId.of(systemUser), UserId.of(managedUser1)).inOrder();
    }

    @Test
    public void testGetUserIds_systemUserAndManagedUser_currentUserIsManagedUser() {
        // Returns the both if there are system and managed users.
        UserId currentUser = UserId.of(managedUser1);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser, managedUser1));
        assertThat(mUserIdManager.getUserIds())
                .containsExactly(UserId.of(systemUser), UserId.of(managedUser1)).inOrder();
    }

    @Test
    public void testGetUserIds_managedUserAndSystemUser_currentUserIsSystemUser() {
        // Returns the both if there are system and managed users, regardless of input list order.
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(managedUser1, systemUser));
        assertThat(mUserIdManager.getUserIds())
                .containsExactly(UserId.of(systemUser), UserId.of(managedUser1)).inOrder();
    }

    // other cases for getUserIds
    @Test
    public void testGetUserIds_NormalUser1AndNormalUser2_currentUserIsNormalUser2() {
        // When there is no managed user, returns the current user.
        UserId currentUser = UserId.of(nonManagedUser2);
        initializeUserIdManager(currentUser, Arrays.asList(nonManagedUser1, nonManagedUser2));
        assertThat(mUserIdManager.getUserIds()).containsExactly(UserId.of(nonManagedUser2));
    }

    @Test
    public void testGetUserIds_NormalUserAndManagedUser_currentUserIsNormalUser() {
        // When there is no system user, returns the current user.
        // This is a case theoretically can happen but we don't expect. So we return the current
        // user only.
        UserId currentUser = UserId.of(nonManagedUser1);
        initializeUserIdManager(currentUser, Arrays.asList(nonManagedUser1, managedUser1));
        assertThat(mUserIdManager.getUserIds()).containsExactly(UserId.of(nonManagedUser1));
    }

    @Test
    public void testGetUserIds_NormalUserAndManagedUser_currentUserIsManagedUser() {
        // When there is no system user, returns the current user.
        // This is a case theoretically can happen but we don't expect. So we return the current
        // user only.
        UserId currentUser = UserId.of(managedUser1);
        initializeUserIdManager(currentUser, Arrays.asList(nonManagedUser1, managedUser1));
        assertThat(mUserIdManager.getUserIds()).containsExactly(UserId.of(managedUser1));
    }

    @Test
    public void testGetUserIds_deviceNotSupported() {
        // we should return the current user when device is not supported.
        UserId currentUser = UserId.of(systemUser);
        when(mockUserManager.getUserProfiles()).thenReturn(Arrays.asList(systemUser, managedUser1));
        mUserIdManager = new UserIdManager.RuntimeUserIdManager(mockContext, currentUser, false);
        assertThat(mUserIdManager.getUserIds()).containsExactly(UserId.of(systemUser));
    }

    @Test
    public void testGetUserIds_deviceWithoutPermission() {
        // This test only tests for Android R or later. This test case always passes before R.
        if (VersionUtils.isAtLeastR()) {
            // When permission is denied, only returns the current user.
            when(mockContext.checkSelfPermission(Manifest.permission.INTERACT_ACROSS_USERS))
                    .thenReturn(PackageManager.PERMISSION_DENIED);
            UserId currentUser = UserId.of(systemUser);
            when(mockUserManager.getUserProfiles()).thenReturn(
                    Arrays.asList(systemUser, managedUser1));
            mUserIdManager = UserIdManager.create(mockContext);
            assertThat(mUserIdManager.getUserIds()).containsExactly(UserId.of(systemUser));
        }
    }

    @Test
    public void testGetUserIds_systemUserAndManagedUser_returnCachedList() {
        // Returns the both if there are system and managed users.
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser, managedUser1));
        assertThat(mUserIdManager.getUserIds()).isSameAs(mUserIdManager.getUserIds());
    }

    @Test
    public void testGetManagedUser_contains() {
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser, managedUser1));
        assertThat(mUserIdManager.getManagedUser()).isEqualTo(UserId.of(managedUser1));
    }

    @Test
    public void testGetSystemUser_contains() {
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser, managedUser1));
        assertThat(mUserIdManager.getSystemUser()).isEqualTo(UserId.of(systemUser));
    }

    @Test
    public void testGetSystemUser_singletonList_returnNull() {
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser));
        assertThat(mUserIdManager.getSystemUser()).isNull();
    }

    @Test
    public void testGetManagedUser_missing() {
        UserId currentUser = UserId.of(systemUser);
        initializeUserIdManager(currentUser, Arrays.asList(systemUser, nonManagedUser1));
        assertThat(mUserIdManager.getManagedUser()).isNull();
    }

    private void initializeUserIdManager(UserId current, List<UserHandle> usersOnDevice) {
        when(mockUserManager.getUserProfiles()).thenReturn(usersOnDevice);
        mUserIdManager = new UserIdManager.RuntimeUserIdManager(mockContext, current, true);
    }
}
