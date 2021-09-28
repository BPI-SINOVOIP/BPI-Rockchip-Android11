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

package com.android.car.settings.testutils;

import android.annotation.UserIdInt;
import android.content.pm.UserInfo;
import android.graphics.Bitmap;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.ArrayMap;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

@Implements(UserManager.class)
public class ShadowUserManager extends org.robolectric.shadows.ShadowUserManager {

    private static boolean sIsHeadlessSystemUserMode = true;
    private static boolean sCanAddMoreUsers = true;
    private static Map<Integer, List<UserInfo>> sProfiles = new ArrayMap<>();
    private static Map<Integer, Bitmap> sUserIcons = new ArrayMap<>();
    private static int sMaxSupportedUsers = 10;

    @Implementation
    protected int[] getProfileIdsWithDisabled(int userId) {
        if (sProfiles.containsKey(userId)) {
            return sProfiles.get(userId).stream().mapToInt(userInfo -> userInfo.id).toArray();
        }
        return new int[]{};
    }

    @Implementation
    protected List<UserInfo> getProfiles(int userHandle) {
        if (sProfiles.containsKey(userHandle)) {
            return new ArrayList<>(sProfiles.get(userHandle));
        }
        return Collections.emptyList();
    }

    /** Adds a profile to be returned by {@link #getProfiles(int)}. **/
    public void addProfile(
            int userHandle, int profileUserHandle, String profileName, int profileFlags) {
        sProfiles.putIfAbsent(userHandle, new ArrayList<>());
        sProfiles.get(userHandle).add(new UserInfo(profileUserHandle, profileName, profileFlags));
    }

    @Implementation
    protected void setUserRestriction(String key, boolean value, UserHandle userHandle) {
        setUserRestriction(userHandle, key, value);
    }

    @Implementation
    protected List<UserInfo> getUsers(boolean excludeDying) {
        return super.getUsers();
    }

    @Implementation
    protected static boolean isHeadlessSystemUserMode() {
        return sIsHeadlessSystemUserMode;
    }

    public static void setIsHeadlessSystemUserMode(boolean isEnabled) {
        sIsHeadlessSystemUserMode = isEnabled;
    }

    @Implementation
    protected static boolean canAddMoreUsers() {
        return sCanAddMoreUsers;
    }

    public static void setCanAddMoreUsers(boolean isEnabled) {
        sCanAddMoreUsers = isEnabled;
    }

    @Implementation
    public Bitmap getUserIcon(@UserIdInt int userId) {
        return sUserIcons.get(userId);
    }

    public static void setUserIcon(@UserIdInt int userId, Bitmap icon) {
        sUserIcons.put(userId, icon);
    }

    @Implementation
    public static int getMaxSupportedUsers() {
        return sMaxSupportedUsers;
    }

    public static void setMaxSupportedUsers(int maxSupportedUsers) {
        sMaxSupportedUsers = maxSupportedUsers;
    }

    @Resetter
    public static void reset() {
        org.robolectric.shadows.ShadowUserManager.reset();
        sIsHeadlessSystemUserMode = true;
        sCanAddMoreUsers = true;
        sProfiles.clear();
        sUserIcons.clear();
    }
}
