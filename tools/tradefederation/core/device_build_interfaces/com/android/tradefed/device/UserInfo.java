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
package com.android.tradefed.device;

/**
 * Similar to UserInfo class from platform.
 *
 * <p>This is intended to be similar to android.content.pm.UserInfo.
 *
 * <p>Stores data and basic logic around the information for one user.
 */
public final class UserInfo {
    // From android.content.pm.UserInfo
    public static final int FLAG_PRIMARY = 0x00000001;
    public static final int FLAG_GUEST = 0x00000004;
    public static final int FLAG_RESTRICTED = 0x00000008;
    public static final int FLAG_MANAGED_PROFILE = 0x00000020;
    public static final int USER_SYSTEM = 0;

    public static final int FLAGS_NOT_SECONDARY =
            FLAG_PRIMARY | FLAG_MANAGED_PROFILE | FLAG_GUEST | FLAG_RESTRICTED;

    private final int mUserId;
    private final String mUserName;
    private final int mFlag;
    private final boolean mIsRunning;

    /** Supported variants of a user's type in external APIs. */
    public enum UserType {
        /** current foreground user of the device */
        CURRENT,
        /**
         * guest user. Only one can exist at a time, may be ephemeral and have more restrictions.
         */
        GUEST,
        /** user flagged as primary on the device; most often primary = system user = user 0 */
        PRIMARY,
        /** system user = user 0 */
        SYSTEM,
        /** secondary user, i.e. non-primary and non-system. */
        SECONDARY,
        /** managed profile user, e.g. work profile. */
        MANAGED_PROFILE;

        public boolean isCurrent() {
            return this == CURRENT;
        }

        public boolean isGuest() {
            return this == GUEST;
        }

        public boolean isPrimary() {
            return this == PRIMARY;
        }

        public boolean isSystem() {
            return this == SYSTEM;
        }

        public boolean isSecondary() {
            return this == SECONDARY;
        }

        public boolean isManagedProfile() {
            return this == MANAGED_PROFILE;
        }
    }

    public UserInfo(int userId, String userName, int flag, boolean isRunning) {
        mUserId = userId;
        mUserName = userName;
        mFlag = flag;
        mIsRunning = isRunning;
    }

    public int userId() {
        return mUserId;
    }

    public String userName() {
        return mUserName;
    }

    public int flag() {
        return mFlag;
    }

    public boolean isRunning() {
        return mIsRunning;
    }

    public boolean isGuest() {
        return (mFlag & FLAG_GUEST) == FLAG_GUEST;
    }

    public boolean isPrimary() {
        return (mFlag & FLAG_PRIMARY) == FLAG_PRIMARY;
    }

    public boolean isSecondary() {
        return !isSystem() && (mFlag & FLAGS_NOT_SECONDARY) == 0;
    }

    public boolean isSystem() {
        return mUserId == USER_SYSTEM;
    }

    public boolean isManagedProfile() {
        return (mFlag & FLAG_MANAGED_PROFILE) == FLAG_MANAGED_PROFILE;
    }

    /** Return whether this instance is of the specified type. */
    public boolean isUserType(UserType userType, int currentUserId) {
        switch (userType) {
            case CURRENT:
                return mUserId == currentUserId;
            case GUEST:
                return isGuest();
            case PRIMARY:
                return isPrimary();
            case SYSTEM:
                return isSystem();
            case SECONDARY:
                return isSecondary();
            case MANAGED_PROFILE:
                return isManagedProfile();
            default:
                throw new RuntimeException("Variant not covered: " + userType);
        }
    }
}
