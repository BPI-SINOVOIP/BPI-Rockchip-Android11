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

import static android.content.pm.UserInfo.FLAG_GUEST;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.UserInfo;
import android.graphics.Bitmap;
import android.os.UserManager;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import com.android.car.settings.testutils.ShadowUserManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserManager.class})
public class UserIconProviderTest {

    private Context mContext;
    private UserIconProvider mUserIconProvider;
    private UserInfo mUserInfo;
    private UserManager mUserManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mUserManager = (UserManager) mContext.getSystemService(Context.USER_SERVICE);

        mUserIconProvider = new UserIconProvider();
        mUserInfo = new UserInfo(/* id= */ 10, "USER_NAME", /* flags= */ 0);
    }

    @After
    public void tearDown() {
        ShadowUserManager.reset();
    }

    @Test
    public void getRoundedUserIcon_AssignsIconIfNotPresent() {
        ShadowUserManager.setUserIcon(mUserInfo.id, null);

        RoundedBitmapDrawable returnedIcon =
                mUserIconProvider.getRoundedUserIcon(mUserInfo, mContext);

        assertThat(returnedIcon).isNotNull();
        assertThat(getShadowUserManager().getUserIcon(mUserInfo.id)).isNotNull();
    }

    @Test
    public void assignDefaultIcon_AssignsIconForNonGuest() {
        ShadowUserManager.setUserIcon(mUserInfo.id, null);

        Bitmap returnedIcon = mUserIconProvider.assignDefaultIcon(
                mUserManager, mContext.getResources(), mUserInfo);

        assertThat(returnedIcon).isNotNull();
        assertThat(getShadowUserManager().getUserIcon(mUserInfo.id)).isNotNull();
    }

    @Test
    public void assignDefaultIcon_AssignsIconForGuest() {
        UserInfo guestUserInfo =
                new UserInfo(/* id= */ 11, "USER_NAME", FLAG_GUEST);
        ShadowUserManager.setUserIcon(guestUserInfo.id, null);

        Bitmap returnedIcon = mUserIconProvider.assignDefaultIcon(
                mUserManager, mContext.getResources(), guestUserInfo);

        assertThat(returnedIcon).isNotNull();
        assertThat(getShadowUserManager().getUserIcon(guestUserInfo.id)).isNotNull();
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadow.extract(mUserManager);
    }
}
