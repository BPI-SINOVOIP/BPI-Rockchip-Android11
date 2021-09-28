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
 * limitations under the License
 */

package com.android.car.settings.security;

import android.os.Bundle;

import androidx.annotation.XmlRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.internal.widget.LockscreenCredential;


/**
 * Give user choices of lock screen type: Pin/Pattern/Password or None.
 */
public class ChooseLockTypeFragment extends SettingsFragment {

    private LockscreenCredential mLockscreenCredential;

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.choose_lock_type_fragment;
    }

    /**
     * Pass along password and password quality to preference controllers
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle args = getArguments();
        if (args != null) {
            mLockscreenCredential = args.getParcelable(PasswordHelper.EXTRA_CURRENT_SCREEN_LOCK);
            int passwordQuality = args.getInt(PasswordHelper.EXTRA_CURRENT_PASSWORD_QUALITY);

            PreferenceScreen preferenceScreen = getPreferenceScreen();
            int preferenceCount = preferenceScreen.getPreferenceCount();
            for (int i = 0; i < preferenceCount; i++) {
                Preference preference = preferenceScreen.getPreference(i);
                preference.getExtras().putParcelable(
                        PasswordHelper.EXTRA_CURRENT_SCREEN_LOCK, mLockscreenCredential);
                preference.getExtras().putInt(
                        PasswordHelper.EXTRA_CURRENT_PASSWORD_QUALITY, passwordQuality);
            }
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        PasswordHelper.zeroizeCredentials(mLockscreenCredential);
    }
}
