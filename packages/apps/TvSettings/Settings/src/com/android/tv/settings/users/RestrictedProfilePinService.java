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

package com.android.tv.settings.users;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.IBinder;
import android.os.UserHandle;

/**
 * Service for storing the exit pin for restricted profiles. This service is only to be used by the
 * {@link RestrictedProfilePinStorage}
 *
 * The pin is stored in the Settings app's shared preferences of the system user. This is only
 * accessible to the Settings app in the system user, which makes it a safe place for storing
 * the pin.
 */
public class RestrictedProfilePinService extends Service {
    private static final String TAG = RestrictedProfilePinService.class.getSimpleName();

    public static final String PIN_STORE_NAME = "restricted_profile_pin";

    private IRestrictedProfilePinService.Stub mBinder;

    @Override
    public void onCreate() {
        mBinder = isSystemUser() ? new PinServiceImpl(this) : null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onDestroy() {
        mBinder = null;
    }

    private boolean isSystemUser() {
        return UserHandle.myUserId() == UserHandle.USER_SYSTEM;
    }

    private static class PinServiceImpl extends IRestrictedProfilePinService.Stub {
        private SharedPreferences mSharedPref;

        PinServiceImpl(Context context) {
            mSharedPref = context.getSharedPreferences(PIN_STORE_NAME, Context.MODE_PRIVATE);
        }

        @Override
        public boolean isPinCorrect(String pin) {
            String savedPin = getPin();
            return pin.equals(savedPin);
        }

        @Override
        public void setPin(String pin) {
            mSharedPref.edit()
                    .putString(PIN_STORE_NAME, pin)
                    .apply();
        }

        @Override
        public void deletePin() {
            mSharedPref.edit()
                    .remove(PIN_STORE_NAME)
                    .apply();
        }

        @Override
        public boolean isPinSet() {
            String savedPin = getPin();
            return savedPin != null;
        }

        private String getPin() {
            return mSharedPref.getString(PIN_STORE_NAME, null);
        }
    }
}
