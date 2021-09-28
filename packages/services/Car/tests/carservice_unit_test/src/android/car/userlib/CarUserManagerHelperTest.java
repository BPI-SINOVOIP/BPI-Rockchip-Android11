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

package android.car.userlib;

import static android.car.test.mocks.AndroidMockitoHelper.mockUmGetSystemUser;
import static android.car.test.mocks.AndroidMockitoHelper.mockUmGetUserInfo;
import static android.car.test.mocks.AndroidMockitoHelper.mockUmGetUsers;
import static android.car.test.util.UserTestingHelper.newUser;
import static android.os.UserHandle.USER_SYSTEM;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.verify;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.car.settings.CarSettings;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.os.UserHandle;
import android.os.UserManager;
import android.sysprop.CarProperties;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;

import java.util.Optional;

/**
 * This class contains unit tests for the {@link CarUserManagerHelper}.
 * It tests that {@link CarUserManagerHelper} does the right thing for user management flows.
 *
 * The following mocks are used:
 * 1. {@link Context} provides system services and resources.
 * 2. {@link UserManager} provides dummy users and user info.
 * 3. {@link ActivityManager} to verify user switch is invoked.
 */
@SmallTest
public class CarUserManagerHelperTest extends AbstractExtendedMockitoTestCase {
    @Mock private Context mContext;
    @Mock private UserManager mUserManager;
    @Mock private ActivityManager mActivityManager;
    @Mock private ContentResolver mContentResolver;

    // Not worth to mock because it would need to mock a Drawable used by UserIcons.
    private final Resources mResources = InstrumentationRegistry.getTargetContext().getResources();

    private static final String TEST_USER_NAME = "testUser";
    private static final int NO_FLAGS = 0;

    private CarUserManagerHelper mCarUserManagerHelper;
    private final int mForegroundUserId = 42;

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session
            .spyStatic(ActivityManager.class)
            .spyStatic(CarProperties.class)
            .spyStatic(UserManager.class);
    }

    @Before
    public void setUpMocksAndVariables() {
        doReturn(mUserManager).when(mContext).getSystemService(Context.USER_SERVICE);
        doReturn(mActivityManager).when(mContext).getSystemService(Context.ACTIVITY_SERVICE);
        doReturn(mResources).when(mContext).getResources();
        doReturn(mContentResolver).when(mContext).getContentResolver();
        doReturn(mContext).when(mContext).getApplicationContext();
        mCarUserManagerHelper = new CarUserManagerHelper(mContext);

        mockGetCurrentUser(mForegroundUserId);
    }

    @Test
    public void testCreateNewNonAdminUser() {
        // Verify createUser on UserManager gets called.
        mCarUserManagerHelper.createNewNonAdminUser(TEST_USER_NAME);
        verify(mUserManager).createUser(TEST_USER_NAME, NO_FLAGS);

        doReturn(null).when(mUserManager).createUser(TEST_USER_NAME, NO_FLAGS);
        assertThat(mCarUserManagerHelper.createNewNonAdminUser(TEST_USER_NAME)).isNull();

        UserInfo newUser = new UserInfo();
        newUser.name = TEST_USER_NAME;
        doReturn(newUser).when(mUserManager).createUser(TEST_USER_NAME, NO_FLAGS);
        assertThat(mCarUserManagerHelper.createNewNonAdminUser(TEST_USER_NAME)).isEqualTo(newUser);
    }

    @Test
    public void testSwitchToId() {
        int userIdToSwitchTo = mForegroundUserId + 2;
        doReturn(true).when(mActivityManager).switchUser(userIdToSwitchTo);

        assertThat(mCarUserManagerHelper.switchToUserId(userIdToSwitchTo)).isTrue();
        verify(mActivityManager).switchUser(userIdToSwitchTo);
    }

    @Test
    public void testSwitchToForegroundIdExitsEarly() {
        doReturn(true).when(mActivityManager).switchUser(mForegroundUserId);

        assertThat(mCarUserManagerHelper.switchToUserId(mForegroundUserId)).isFalse();
        verify(mActivityManager, never()).switchUser(mForegroundUserId);
    }

    @Test
    public void testCannotSwitchIfSwitchingNotAllowed() {
        int userIdToSwitchTo = mForegroundUserId + 2;
        doReturn(true).when(mActivityManager).switchUser(userIdToSwitchTo);
        doReturn(UserManager.SWITCHABILITY_STATUS_USER_SWITCH_DISALLOWED)
                .when(mUserManager).getUserSwitchability();
        assertThat(mCarUserManagerHelper.switchToUserId(userIdToSwitchTo)).isFalse();
        verify(mActivityManager, never()).switchUser(userIdToSwitchTo);
    }

    @Test
    public void testGrantAdminPermissions() {
        int userId = 30;
        UserInfo testInfo = newUser(userId);

        // Test that non-admins cannot grant admin permissions.
        doReturn(false).when(mUserManager).isAdminUser(); // Current user non-admin.
        mCarUserManagerHelper.grantAdminPermissions(testInfo);
        verify(mUserManager, never()).setUserAdmin(userId);

        // Admins can grant admin permissions.
        doReturn(true).when(mUserManager).isAdminUser();
        mCarUserManagerHelper.grantAdminPermissions(testInfo);
        verify(mUserManager).setUserAdmin(userId);
    }

    @Test
    public void testDefaultNonAdminRestrictions() {
        String testUserName = "Test User";
        int userId = 20;
        UserInfo newNonAdmin = newUser(userId);

        doReturn(newNonAdmin).when(mUserManager).createUser(testUserName, NO_FLAGS);

        mCarUserManagerHelper.createNewNonAdminUser(testUserName);

        verify(mUserManager).setUserRestriction(
                UserManager.DISALLOW_FACTORY_RESET, /* enable= */ true, UserHandle.of(userId));
    }

    @Test
    public void testGrantingAdminPermissionsRemovesNonAdminRestrictions() {
        int testUserId = 30;
        boolean restrictionEnabled = false;
        UserInfo testInfo = newUser(testUserId);

        // Only admins can grant permissions.
        doReturn(true).when(mUserManager).isAdminUser();

        mCarUserManagerHelper.grantAdminPermissions(testInfo);

        verify(mUserManager).setUserRestriction(
                UserManager.DISALLOW_FACTORY_RESET, restrictionEnabled, UserHandle.of(testUserId));
    }

    @Test
    public void testGetInitialUser_WithValidLastActiveUser_ReturnsLastActiveUser() {
        setLastActiveUser(12);
        mockGetUsers(USER_SYSTEM, 10, 11, 12);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(12);
    }

    @Test
    public void testGetInitialUser_WithNonExistLastActiveUser_ReturnsLastPersistentUser() {
        setLastActiveUser(120);
        setLastPersistentActiveUser(110);
        mockGetUsers(USER_SYSTEM, 100, 110);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(110);
        // should have reset last active user
        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID))
                .isEqualTo(UserHandle.USER_NULL);
    }

    @Test
    public void testGetInitialUser_WithNonExistLastActiveAndPersistentUsers_ReturnsSmallestUser() {
        setLastActiveUser(120);
        setLastPersistentActiveUser(120);
        mockGetUsers(USER_SYSTEM, 100, 110);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(100);
        // should have reset both settions
        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID))
                .isEqualTo(UserHandle.USER_NULL);
        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID))
                .isEqualTo(UserHandle.USER_NULL);
    }

    @Test
    public void testGetInitialUser_WithOverrideId_ReturnsOverrideId() {
        setDefaultBootUserOverride(11);
        setLastActiveUser(12);
        mockGetUsers(USER_SYSTEM, 10, 11, 12);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(11);
    }

    @Test
    public void testGetInitialUser_WithInvalidOverrideId_ReturnsLastActiveUserId() {
        setDefaultBootUserOverride(15);
        setLastActiveUser(12);
        mockGetUsers(USER_SYSTEM, 10, 11, 12);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(12);
    }

    @Test
    public void testGetInitialUser_WithInvalidOverrideAndLastActiveUserIds_ReturnsSmallestUserId() {
        int minimumUserId = 10;
        int invalidLastActiveUserId = 14;
        int invalidOverrideUserId = 15;

        setDefaultBootUserOverride(invalidOverrideUserId);
        setLastActiveUser(invalidLastActiveUserId);
        mockGetUsers(USER_SYSTEM, minimumUserId, minimumUserId + 1, minimumUserId + 2);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(minimumUserId);
    }

    @Test
    public void testGetInitialUser_WhenOverrideIdIsIgnored() {
        setDefaultBootUserOverride(11);
        setLastActiveUser(12);
        mockGetUsers(USER_SYSTEM, 10, 11, 12);

        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ false))
                .isEqualTo(12);
    }

    @Test
    public void testGetInitialUser_WithEmptyReturnNull() {
        assertThat(mCarUserManagerHelper.getInitialUser(/* usesOverrideUserIdProperty= */ true))
                .isEqualTo(UserHandle.USER_NULL);
    }

    @Test
    public void testHasInitialUser_onlyHeadlessSystemUser() {
        mockIsHeadlessSystemUserMode(true);
        mockGetUsers(USER_SYSTEM);

        assertThat(mCarUserManagerHelper.hasInitialUser()).isFalse();
    }

    @Test
    public void testHasInitialUser_onlyNonHeadlessSystemUser() {
        mockIsHeadlessSystemUserMode(false);
        mockGetUsers(USER_SYSTEM);

        assertThat(mCarUserManagerHelper.hasInitialUser()).isTrue();
    }

    @Test
    public void testHasInitialUser_hasNormalUser() {
        mockIsHeadlessSystemUserMode(true);
        mockGetUsers(USER_SYSTEM, 10);

        assertThat(mCarUserManagerHelper.hasInitialUser()).isTrue();
    }

    @Test
    public void testHasInitialUser_hasOnlyWorkProfile() {
        mockIsHeadlessSystemUserMode(true);

        UserInfo systemUser = newUser(UserHandle.USER_SYSTEM);

        UserInfo workProfile = newUser(10);
        workProfile.userType = UserManager.USER_TYPE_PROFILE_MANAGED;
        assertThat(workProfile.isManagedProfile()).isTrue(); // Sanity check

        mockGetUsers(systemUser, workProfile);

        assertThat(mCarUserManagerHelper.hasInitialUser()).isFalse();
    }

    @Test
    public void testSetLastActiveUser_headlessSystem() {
        mockIsHeadlessSystemUserMode(true);
        mockUmGetSystemUser(mUserManager);

        mCarUserManagerHelper.setLastActiveUser(UserHandle.USER_SYSTEM);

        assertSettingsNotSet(CarSettings.Global.LAST_ACTIVE_USER_ID);
        assertSettingsNotSet(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID);
    }

    @Test
    public void testSetLastActiveUser_nonHeadlessSystem() {
        mockIsHeadlessSystemUserMode(false);
        mockUmGetSystemUser(mUserManager);

        mCarUserManagerHelper.setLastActiveUser(UserHandle.USER_SYSTEM);

        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID))
                .isEqualTo(UserHandle.USER_SYSTEM);
        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID))
                .isEqualTo(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testSetLastActiveUser_nonExistingUser() {
        // Don't need to mock um.getUser(), it will return null by default
        mCarUserManagerHelper.setLastActiveUser(42);

        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID)).isEqualTo(42);
        assertSettingsNotSet(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID);
    }

    @Test
    public void testSetLastActiveUser_ephemeralUser() {
        int persistentUserId = 42;
        int ephemeralUserid = 108;
        mockUmGetUserInfo(mUserManager, persistentUserId, NO_FLAGS);
        mockUmGetUserInfo(mUserManager, ephemeralUserid, UserInfo.FLAG_EPHEMERAL);

        mCarUserManagerHelper.setLastActiveUser(persistentUserId);
        mCarUserManagerHelper.setLastActiveUser(ephemeralUserid);

        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID))
                .isEqualTo(ephemeralUserid);
        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID))
                .isEqualTo(persistentUserId);
    }

    @Test
    public void testSetLastActiveUser_nonEphemeralUser() {
        mockUmGetUserInfo(mUserManager, 42, NO_FLAGS);

        mCarUserManagerHelper.setLastActiveUser(42);

        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID)).isEqualTo(42);
        assertThat(getSettingsInt(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID)).isEqualTo(42);
    }

    private void mockGetUsers(@NonNull @UserIdInt int... userIds) {
        mockUmGetUsers(mUserManager, userIds);
    }

    private void mockGetUsers(@NonNull UserInfo... users) {
        mockUmGetUsers(mUserManager, users);
    }

    private void setLastActiveUser(@UserIdInt int userId) {
        putSettingsInt(CarSettings.Global.LAST_ACTIVE_USER_ID, userId);
    }

    private void setLastPersistentActiveUser(@UserIdInt int userId) {
        putSettingsInt(CarSettings.Global.LAST_ACTIVE_PERSISTENT_USER_ID, userId);
    }

    private void setDefaultBootUserOverride(@UserIdInt int userId) {
        doReturn(Optional.of(userId)).when(() -> CarProperties.boot_user_override_id());
    }
}
