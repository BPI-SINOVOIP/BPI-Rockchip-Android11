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

package android.car.userlib;

import static android.car.test.util.UserTestingHelper.newUser;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.content.Context;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;

public final class UserHelperTest extends AbstractExtendedMockitoTestCase {

    @Mock private Context mContext;
    @Mock private UserManager mUserManager;

    // Not worth to mock because it would need to mock a Drawable used by UserIcons.
    private final Resources mResources = InstrumentationRegistry.getTargetContext().getResources();

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session.spyStatic(UserManager.class);
    }

    @Before
    public void setUp() {
        when(mContext.getSystemService(Context.USER_SERVICE)).thenReturn(mUserManager);
        when(mContext.getApplicationContext()).thenReturn(mContext);
        when(mContext.getResources()).thenReturn(mResources);
    }

    @Test
    public void testSafeName() {
        assertThat(UserHelper.safeName(null)).isNull();

        String safe = UserHelper.safeName("UnsafeIam");
        assertThat(safe).isNotNull();
        assertThat(safe).doesNotContain("UnsafeIAm");
    }

    @Test
    public void testIsHeadlessSystemUser_system_headlessMode() {
        mockIsHeadlessSystemUserMode(true);
        assertThat(UserHelper.isHeadlessSystemUser(UserHandle.USER_SYSTEM)).isTrue();
    }

    @Test
    public void testIsHeadlessSystemUser_system_nonHeadlessMode() {
        mockIsHeadlessSystemUserMode(false);
        assertThat(UserHelper.isHeadlessSystemUser(UserHandle.USER_SYSTEM)).isFalse();
    }

    @Test
    public void testIsHeadlessSystemUser_nonSystem_headlessMode() {
        mockIsHeadlessSystemUserMode(true);
        assertThat(UserHelper.isHeadlessSystemUser(10)).isFalse();
    }

    @Test
    public void testIsHeadlessSystemUser_nonSystem_nonHeadlessMode() {
        mockIsHeadlessSystemUserMode(false);
        assertThat(UserHelper.isHeadlessSystemUser(10)).isFalse();
    }

    @Test
    public void testDefaultNonAdminRestrictions() {
        int userId = 20;
        UserInfo newNonAdmin = newUser(userId);

        UserHelper.setDefaultNonAdminRestrictions(mContext, newNonAdmin, /* enable= */ true);

        verify(mUserManager).setUserRestriction(
                UserManager.DISALLOW_FACTORY_RESET, /* enable= */ true, UserHandle.of(userId));
    }

    @Test
    public void testDefaultNonAdminRestrictions_nullContext_throwsException() {
        int userId = 20;
        UserInfo newNonAdmin = newUser(userId);

        assertThrows(IllegalArgumentException.class,
                () -> UserHelper.setDefaultNonAdminRestrictions(/* context= */ null,
                        newNonAdmin, /* enable= */ true));
    }

    @Test
    public void testDefaultNonAdminRestrictions_nullUser_throwsException() {
        assertThrows(IllegalArgumentException.class,
                () -> UserHelper.setDefaultNonAdminRestrictions(mContext, /* user= */
                        null, /* enable= */ true));
    }

    @Test
    public void testAssignDefaultIcon() {
        int userId = 20;
        UserInfo newNonAdmin = newUser(userId);

        Bitmap bitmap = UserHelper.assignDefaultIcon(mContext, newNonAdmin);

        verify(mUserManager).setUserIcon(userId, bitmap);
    }

    @Test
    public void testAssignDefaultIcon_nullContext_throwsException() {
        int userId = 20;
        UserInfo newNonAdmin = newUser(userId);

        assertThrows(IllegalArgumentException.class,
                () -> UserHelper.assignDefaultIcon(/* context= */ null, newNonAdmin));
    }

    @Test
    public void testAssignDefaultIcon_nullUser_throwsException() {
        assertThrows(IllegalArgumentException.class,
                () -> UserHelper.assignDefaultIcon(mContext, /* user= */ null));
    }
}
