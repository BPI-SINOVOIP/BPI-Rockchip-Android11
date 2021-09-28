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

import android.car.userlib.CarUserManagerHelper;
import android.content.pm.UserInfo;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.HashMap;
import java.util.Map;

/**
 * Shadow for {@link CarUserManagerHelper}
 */
@Implements(CarUserManagerHelper.class)
public class ShadowCarUserManagerHelper {
    // First Map keys from user id to map of restrictions. Second Map keys from restriction id to
    // bool.
    private static Map<Integer, Map<String, Boolean>> sUserRestrictionMap = new HashMap<>();
    private static CarUserManagerHelper sMockInstance;

    public static void setMockInstance(CarUserManagerHelper userManagerHelper) {
        sMockInstance = userManagerHelper;
    }

    @Resetter
    public static void reset() {
        sMockInstance = null;
        sUserRestrictionMap.clear();
    }

    @Implementation
    protected UserInfo createNewNonAdminUser(String userName) {
        return sMockInstance.createNewNonAdminUser(userName);
    }

    @Implementation
    protected void grantAdminPermissions(UserInfo user) {
        sMockInstance.grantAdminPermissions(user);
    }

    @Implementation
    protected void setUserRestriction(UserInfo userInfo, String restriction, boolean enable) {
        Map<String, Boolean> permissionsMap = sUserRestrictionMap.getOrDefault(userInfo.id,
                new HashMap<>());
        permissionsMap.put(restriction, enable);
        sUserRestrictionMap.put(userInfo.id, permissionsMap);
    }

    @Implementation
    protected boolean hasUserRestriction(String restriction, UserInfo userInfo) {
        // False by default, if nothing was added to our map,
        if (!sUserRestrictionMap.containsKey(userInfo.id)) {
            return false;
        }
        return sUserRestrictionMap.get(userInfo.id).getOrDefault(restriction, false);
    }
}
