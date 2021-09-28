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

package com.android.car.settings.security;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;

import androidx.fragment.app.Fragment;
import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.internal.widget.LockscreenCredential;

/**
 * Business Logic for security lock preferences. It can be extended to change which fragment should
 * be opened when clicked.
 */
public abstract class LockTypeBasePreferenceController extends PreferenceController<Preference> {

    private final UserManager mUserManager;
    private LockscreenCredential mCurrentPassword;
    private int mCurrentPasswordQuality;

    public LockTypeBasePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mUserManager = UserManager.get(context);
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    /**
     * Fragment specified here will be opened when the Preference is clicked. Return null to prevent
     * navigation.
     */
    protected abstract Fragment fragmentToOpen();

    /**
     * If the current password quality is one of the values returned by this function, the
     * controller will identify as having the current lock.
     */
    protected abstract int[] allowedPasswordQualities();


    /** Sets the quality of the current password. */
    public void setCurrentPasswordQuality(int currentPasswordQuality) {
        mCurrentPasswordQuality = currentPasswordQuality;
    }

    /** Gets whether the preference related to this controller is the current lock type. */
    protected boolean isCurrentLock() {
        for (int allowedQuality : allowedPasswordQualities()) {
            if (mCurrentPasswordQuality == allowedQuality) {
                return true;
            }
        }
        return false;
    }

    /**
     * Sets the current password so it can be provided in the bundle in the fragment. The host
     * fragment is responsible for zeroizing the credentials in memory. */
    public void setCurrentPassword(LockscreenCredential currentPassword) {
        mCurrentPassword = currentPassword;
    }

    /** Gets the current password. */
    protected LockscreenCredential getCurrentPassword() {
        return mCurrentPassword;
    }

    /** Gets the current password quality. */
    protected int getCurrentPasswordQuality() {
        return mCurrentPasswordQuality;
    }

    @Override
    protected void updateState(Preference preference) {
        preference.setSummary(getSummary());
    }

    @Override
    protected boolean handlePreferenceClicked(Preference preference) {
        Fragment fragment = fragmentToOpen();
        if (fragment != null) {
            if (mCurrentPassword != null) {
                Bundle args = fragment.getArguments();
                if (args == null) {
                    args = new Bundle();
                }
                args.putParcelable(PasswordHelper.EXTRA_CURRENT_SCREEN_LOCK, mCurrentPassword);
                fragment.setArguments(args);
            }
            getFragmentController().launchFragment(fragment);
            return true;
        }
        return false;
    }

    /**
     * Retrieve and set password and password quality
     */
    @Override
    protected void onCreateInternal() {
        if (getPreference().peekExtras() != null) {
            setCurrentPassword(getPreference().peekExtras().getParcelable(
                    PasswordHelper.EXTRA_CURRENT_SCREEN_LOCK));
            setCurrentPasswordQuality(getPreference().peekExtras().getInt(
                    PasswordHelper.EXTRA_CURRENT_PASSWORD_QUALITY));
        }
    }

    @Override
    public int getAvailabilityStatus() {
        return mUserManager.isGuestUser() ? DISABLED_FOR_USER : AVAILABLE;
    }

    private CharSequence getSummary() {
        return isCurrentLock() ? getContext().getString(R.string.current_screen_lock) : "";
    }
}
