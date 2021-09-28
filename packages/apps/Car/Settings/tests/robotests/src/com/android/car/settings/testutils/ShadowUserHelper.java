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

package com.android.car.settings.testutils;

import android.annotation.Nullable;
import android.content.Context;
import android.content.pm.UserInfo;

import com.android.car.settings.users.UserHelper;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.List;
import java.util.function.Predicate;

/**
 * Shadow for {@link UserHelper}.
 */
@Implements(UserHelper.class)
public class ShadowUserHelper {
    private static UserHelper sInstance;

    public static void setInstance(UserHelper userHelper) {
        sInstance = userHelper;
    }

    @Resetter
    public static void reset() {
        sInstance = null;
    }

    @Implementation
    protected boolean canCurrentProcessModifyAccounts() {
        return sInstance.canCurrentProcessModifyAccounts();
    }

    @Implementation
    protected List<UserInfo> getAllUsers() {
        return sInstance.getAllUsers();
    }

    @Implementation
    protected List<UserInfo> getAllSwitchableUsers() {
        return sInstance.getAllSwitchableUsers();
    }


    @Implementation
    protected List<UserInfo> getAllPersistentUsers() {
        return sInstance.getAllPersistentUsers();
    }

    @Implementation
    public boolean isCurrentProcessUser(UserInfo userInfo) {
        return sInstance.isCurrentProcessUser(userInfo);
    }

    @Implementation
    protected UserInfo getCurrentProcessUserInfo() {
        return sInstance.getCurrentProcessUserInfo();
    }

    @Implementation
    public boolean removeUser(Context context, UserInfo userInfo) {
        return sInstance.removeUser(context, userInfo);
    }

    @Implementation
    public UserInfo createNewOrFindExistingGuest(Context context) {
        return sInstance.createNewOrFindExistingGuest(context);
    }

    @Implementation
    public List<UserInfo> getAllLivingUsers(@Nullable Predicate<? super UserInfo> filter) {
        return sInstance.getAllLivingUsers(filter);
    }
}