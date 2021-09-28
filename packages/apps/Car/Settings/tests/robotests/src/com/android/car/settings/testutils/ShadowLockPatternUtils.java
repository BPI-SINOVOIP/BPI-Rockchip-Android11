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

import android.app.admin.DevicePolicyManager;

import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockscreenCredential;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

/**
 * Shadow for LockPatternUtils.
 */
@Implements(LockPatternUtils.class)
public class ShadowLockPatternUtils {

    public static final int NO_USER = -1;

    private static int sPasswordQuality = DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED;
    private static byte[] sSavedPassword;
    private static byte[] sSavedPattern;
    private static LockscreenCredential sClearLockCredential;
    private static int sClearLockUser = NO_USER;


    @Resetter
    public static void reset() {
        sPasswordQuality = DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED;
        sSavedPassword = null;
        sSavedPattern = null;
        sClearLockCredential = null;
        sClearLockUser = NO_USER;
    }

    /**
     * Sets the current password quality that is returned by
     * {@link LockPatternUtils#getKeyguardStoredPasswordQuality}.
     */
    public static void setPasswordQuality(int passwordQuality) {
        sPasswordQuality = passwordQuality;
    }

    /**
     * Returns the password saved by a call to {@link LockPatternUtils#saveLockPassword}.
     */
    public static byte[] getSavedPassword() {
        return sSavedPassword;
    }

    /**
     * Returns the saved credential passed in to clear the lock, null if it has not been cleared.
     */
    public static LockscreenCredential getClearLockCredential() {
        return sClearLockCredential;
    }

    /**
     * Returns the user passed in to clear the lock, {@link #NO_USER} if it has not been cleared.
     */
    public static int getClearLockUser() {
        return sClearLockUser;
    }


    /**
     * Returns the pattern saved by a call to {@link LockPatternUtils#saveLockPattern}.
     */
    public static byte[] getSavedPattern() {
        return sSavedPattern;
    }

    @Implementation
    protected int getKeyguardStoredPasswordQuality(int userHandle) {
        return sPasswordQuality;
    }

    @Implementation
    public boolean setLockCredential(LockscreenCredential newCredential,
            LockscreenCredential savedCredential, int userId) {
        if (newCredential.isPassword() || newCredential.isPin()) {
            sSavedPassword = newCredential.duplicate().getCredential();
        } else if (newCredential.isPattern()) {
            sSavedPattern = newCredential.duplicate().getCredential();
        } else if (newCredential.isNone()) {
            sClearLockCredential = savedCredential.duplicate();
            sClearLockUser = userId;
        }
        return true;
    }
}
