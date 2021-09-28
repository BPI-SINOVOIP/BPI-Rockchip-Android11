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

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.content.Context;
import android.content.pm.UserInfo;
import android.graphics.Bitmap;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.internal.util.Preconditions;
import com.android.internal.util.UserIcons;

import com.google.android.collect.Sets;

import java.util.Set;

/**
 * Provides utility methods for generic user-related functionalities that don't require a manager.
 */
public final class UserHelper {

    /**
     * Default set of restrictions for Non-Admin users.
     */
    private static final Set<String> DEFAULT_NON_ADMIN_RESTRICTIONS = Sets.newArraySet(
            UserManager.DISALLOW_FACTORY_RESET
    );

    private UserHelper() {
        throw new UnsupportedOperationException("contains only static methods");
    }

    /**
     * Gets a PII-safe representation of the name.
     */
    @Nullable
    public static String safeName(@Nullable String name) {
        return name == null ? name : name.length() + "_chars";
    }

    /**
     * Checks whether the given user is both {@code SYSTEM} and headless.
     */
    public static boolean isHeadlessSystemUser(@UserIdInt int userId) {
        return userId == UserHandle.USER_SYSTEM && UserManager.isHeadlessSystemUserMode();
    }

    /**
     * Sets the values of default Non-Admin restrictions to the passed in value.
     *
     * @param context Current application context
     * @param user User to set restrictions on.
     * @param enable If true, restriction is ON, If false, restriction is OFF.
     */
    public static void setDefaultNonAdminRestrictions(@NonNull Context context,
            @NonNull UserInfo user, boolean enable) {
        Preconditions.checkArgument(context != null, "Context cannot be null");
        Preconditions.checkArgument(user != null, "User cannot be null");

        UserManager userManager = UserManager.get(context);
        for (String restriction : DEFAULT_NON_ADMIN_RESTRICTIONS) {
            userManager.setUserRestriction(restriction, enable, user.getUserHandle());
        }
    }

    /**
     * Assigns a default icon to a user according to the user's id.
     *
     * @param context Current application context
     * @param user User whose avatar is set to default icon.
     * @return Bitmap of the user icon.
     */
    @NonNull
    public static Bitmap assignDefaultIcon(@NonNull Context context, @NonNull UserInfo user) {
        Preconditions.checkArgument(context != null, "Context cannot be null");
        Preconditions.checkArgument(user != null, "User cannot be null");

        int idForIcon = user.isGuest() ? UserHandle.USER_NULL : user.id;
        Bitmap bitmap = UserIcons.convertToBitmap(
                UserIcons.getDefaultUserIcon(context.getResources(), idForIcon, false));
        UserManager.get(context).setUserIcon(user.id, bitmap);
        return bitmap;
    }
}
