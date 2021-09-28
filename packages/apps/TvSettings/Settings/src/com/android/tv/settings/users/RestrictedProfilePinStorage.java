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

package com.android.tv.settings.users;

import android.content.Context;
import android.content.pm.UserInfo;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockPatternUtils.RequestThrottledException;
import com.android.internal.widget.LockscreenCredential;

/**
 * Utilities for saving restricted profile PINs. This class binds to a
 * {@link RestrictedProfilePinService} to store and retrieve pins.
 */
public class RestrictedProfilePinStorage {
    private static final String TAG = RestrictedProfilePinStorage.class.getSimpleName();

    private final int mOwnerUserId;
    private final LockPatternUtils mLockPatternUtils;

    private final RestrictedProfilePinServiceConnection mConnection;

    /**
     * Return a new instance of RestrictedProfilePinStorage.
     */
    public static RestrictedProfilePinStorage newInstance(Context context) {
        LockPatternUtils lpu = new LockPatternUtils(context);

        UserManager um = (UserManager) context.getSystemService(Context.USER_SERVICE);
        UserInfo currentUserInfo = um.getUserInfo(context.getUserId());

        final int ownerUserId = findRestrictedProfileParent(currentUserInfo);

        RestrictedProfilePinServiceConnection connection =
                new RestrictedProfilePinServiceConnection(context);
        return new RestrictedProfilePinStorage(lpu, ownerUserId, connection);
    }

    @VisibleForTesting
    RestrictedProfilePinStorage(LockPatternUtils lpu, int ownerUserId,
            RestrictedProfilePinServiceConnection connection) {
        mLockPatternUtils = lpu;
        mOwnerUserId = ownerUserId;
        mConnection = connection;
    }

    /** Binds the pin service to be used later for all pin operations. To be called before any
     * other use of this class. */
    public void bind() {
        mConnection.bindPinService();
    }

    /** Unbinds the pin service. */
    public void unbind() {
        mConnection.unbindPinService();
    }

    /**
     * Set PIN password for the profile.
     * @param pin New PIN password
     * @param originalPin Original PIN password
     */
    boolean setPin(@NonNull String pin, String originalPin) {
        if (!isPinSet() || isPinCorrect(originalPin)) {
            setPinInternal(pin);
            clearLockPassword(LockscreenCredential.createPinOrNone(originalPin));
            return true;
        } else {
            Log.w(TAG, "Unable to validate the original PIN");
            return false;
        }
    }

    /**
     * Delete PIN password for the profile.
     */
    boolean deletePin(@NonNull String oldPin) {
        if (isPinCorrect(oldPin)) {
            deletePinInternal();
            clearLockPassword(LockscreenCredential.createPinOrNone(oldPin));
            return true;
        } else {
            Log.w(TAG, "Unable to validate the original PIN");
            return false;
        }
    }

    /**
     * Validate PIN password for the profile.
     */
    boolean isPinCorrect(@NonNull String pin) {
        return isPinSetInternal()
                ? isPinCorrectInternal(pin)
                : !hasLockscreenSecurity() || checkPasswordLegacy(pin);
    }

    /**
     * Check if there is a PIN password set on the profile.
     */
    public boolean isPinSet() {
        return isPinSetInternal() || hasLockscreenSecurity();
    }

    private boolean setPinInternal(String pin) {
        if (pin == null) {
            Log.e(TAG, "Attempt to set restricted profile pin to null. Aborting...");
            return false;
        }

        try {
            getPinService().setPin(pin);
            return true;
        } catch (RemoteException e) {
            Log.w(TAG, "Could not set pin due to remote exception");
            e.printStackTrace();
            return false;
        }
    }

    private boolean deletePinInternal() {
        try {
            getPinService().deletePin();
            return true;
        } catch (RemoteException e) {
            Log.w(TAG, "Could not delete pin due to remote exception");
            e.printStackTrace();
            return false;
        }
    }

    private boolean isPinCorrectInternal(String originalPin) {
        try {
            return getPinService().isPinCorrect(originalPin);
        } catch (RemoteException e) {
            Log.w(TAG, "Could not check pin due to remote exception");
            e.printStackTrace();
        }

        return false;
    }

    private boolean isPinSetInternal() {
        try {
            return getPinService().isPinSet();
        } catch (RemoteException e) {
            e.printStackTrace();
        }

        return true;
    }

    private IRestrictedProfilePinService getPinService() throws RemoteException {
        return mConnection.getPinService();
    }

    private void clearLockPassword(LockscreenCredential oldPin) {
        mLockPatternUtils.setLockCredential(LockscreenCredential.createNone(), oldPin,
                mOwnerUserId);
    }

    private boolean checkPasswordLegacy(String pin) {
        try {
            LockscreenCredential credential = LockscreenCredential.createPin(pin);
            Log.i(TAG, "checkPasswordLegacy " + credential.getCredential());
            boolean response = mLockPatternUtils.checkCredential(credential, mOwnerUserId, null);
            Log.i(TAG, "response " + response);
            if (response) {
                // Copy PIN to internal storage.
                setPinInternal(pin);
            }
            return response;
        } catch (RequestThrottledException e) {
            Log.e(TAG, "Unable to check password for unlocking the user", e);
        }
        return false; // Interpret this as an incorrect pin and ask for the pin again.
    }

    private boolean hasLockscreenSecurity() {
        return mLockPatternUtils.isSecure(mOwnerUserId);
    }

    private static int findRestrictedProfileParent(UserInfo currentUserInfo) {
        final int ownerUserId;

        if (!currentUserInfo.isRestricted()) {
            ownerUserId = currentUserInfo.id;
        } else if (currentUserInfo.restrictedProfileParentId == UserInfo.NO_PROFILE_GROUP_ID) {
            ownerUserId = UserHandle.USER_SYSTEM;
        } else {
            ownerUserId = currentUserInfo.restrictedProfileParentId;
        }

        return ownerUserId;
    }
}
