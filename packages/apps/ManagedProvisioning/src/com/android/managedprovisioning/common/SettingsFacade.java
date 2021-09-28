/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.common;

import static android.provider.Settings.Global.DEVICE_PROVISIONED;
import static android.provider.Settings.Secure.MANAGED_PROFILE_CONTACT_REMOTE_SEARCH;
import static android.provider.Settings.Secure.CROSS_PROFILE_CALENDAR_ENABLED;
import static android.provider.Settings.Secure.USER_SETUP_COMPLETE;

import android.content.Context;
import android.provider.Settings.Global;
import android.provider.Settings.Secure;

/**
 * Wrapper class around the static Settings provider calls.
 */
public class SettingsFacade {
    /**
     * Sets USER_SETUP_COMPLETE for a given user.
     */
    public void setUserSetupCompleted(Context context, int userId) {
        ProvisionLogger.logd("Setting USER_SETUP_COMPLETE to 1 for user " + userId);
        Secure.putIntForUser(context.getContentResolver(), USER_SETUP_COMPLETE, 1, userId);
    }

    /**
     * Returns whether USER_SETUP_COMPLETE is set on the calling user.
     */
    public boolean isUserSetupCompleted(Context context) {
        return Secure.getInt(context.getContentResolver(), USER_SETUP_COMPLETE, 0) != 0;
    }

    /**
     * Returns whether DEVICE_PROVISIONED is set.
     */
    public boolean isDeviceProvisioned(Context context) {
        return Global.getInt(context.getContentResolver(), DEVICE_PROVISIONED, 0) != 0;
    }

    /**
     * Sets whether profile contact remote search is enabled.
     */
    public void setProfileContactRemoteSearch(Context context, boolean allowed, int userId) {
        Secure.putIntForUser(context.getContentResolver(),
                MANAGED_PROFILE_CONTACT_REMOTE_SEARCH, allowed ? 1 : 0, userId);
    }

    /**
     * Sets whether cross-profile calendar is enabled.
     */
    public void setCrossProfileCalendarEnabled(Context context, boolean allowed, int userId) {
        Secure.putIntForUser(context.getContentResolver(),
                CROSS_PROFILE_CALENDAR_ENABLED, allowed ? 1 : 0, userId);
    }

    /**
     * Returns whether we are running during the setup wizard flow.
     */
    public boolean isDuringSetupWizard(Context context) {
        // If the flow is running in SUW, the primary user is not set up at this point
        return !isUserSetupCompleted(context);
    }

    /**
     * Returns whether ADB is enabled (developer mode)
     */
    public boolean isDeveloperMode(Context context) {
        return Global.getInt(context.getContentResolver(), Global.ADB_ENABLED, 0) > 0;
    }
}
