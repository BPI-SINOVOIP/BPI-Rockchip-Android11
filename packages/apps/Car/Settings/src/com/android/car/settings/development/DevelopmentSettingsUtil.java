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

package com.android.car.settings.development;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;

import com.android.car.settings.R;
import com.android.car.settings.common.Logger;

/**
 * A utility to set/check development settings mode.
 *
 * <p>Shared logic with {@link com.android.settingslib.development.DevelopmentSettingsEnabler} with
 * modifications to use CarUserManagerHelper instead of UserManager.
 */
public class DevelopmentSettingsUtil {

    private static final Logger LOG = new Logger(DevelopmentSettingsUtil.class);

    private DevelopmentSettingsUtil() {
    }

    /**
     * Sets the global toggle for developer settings and sends out a local broadcast to notify other
     * of this change.
     */
    public static void setDevelopmentSettingsEnabled(Context context, boolean enable) {
        Settings.Global.putInt(context.getContentResolver(),
                Settings.Global.DEVELOPMENT_SETTINGS_ENABLED, enable ? 1 : 0);

        // Enable developer options module.
        setDeveloperOptionsEnabledState(context, showDeveloperOptions(context));
    }

    /**
     * Checks that the development settings should be enabled. Returns true if global toggle is set,
     * debugging is allowed for user, and the user is an admin user.
     */
    public static boolean isDevelopmentSettingsEnabled(Context context, UserManager userManager) {
        boolean settingEnabled = Settings.Global.getInt(context.getContentResolver(),
                Settings.Global.DEVELOPMENT_SETTINGS_ENABLED, Build.IS_ENG ? 1 : 0) != 0;
        boolean hasRestriction = userManager.hasUserRestriction(
                UserManager.DISALLOW_DEBUGGING_FEATURES);
        boolean isAdmin = userManager.isAdminUser();
        return isAdmin && !hasRestriction && settingEnabled;
    }

    /** Checks whether the device is provisioned or not. */
    public static boolean isDeviceProvisioned(Context context) {
        return Settings.Global.getInt(context.getContentResolver(),
                Settings.Global.DEVICE_PROVISIONED, 0) != 0;
    }

    /** Checks whether the developer options module is enabled. */
    public static boolean isDeveloperOptionsModuleEnabled(Context context) {
        PackageManager pm = context.getPackageManager();
        ComponentName component = getDeveloperOptionsModule(context);
        int state = pm.getComponentEnabledSetting(component);
        return state == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
    }

    private static ComponentName getDeveloperOptionsModule(Context context) {
        return ComponentName.unflattenFromString(
                context.getString(R.string.config_dev_options_module));
    }

    private static boolean showDeveloperOptions(Context context) {
        UserManager userManager = UserManager.get(context);
        boolean showDev = isDevelopmentSettingsEnabled(context, userManager)
                && !ActivityManager.isUserAMonkey();
        boolean isAdmin = userManager.isAdminUser();
        if (UserHandle.MU_ENABLED && !isAdmin) {
            showDev = false;
        }

        return showDev;
    }

    private static void setDeveloperOptionsEnabledState(Context context, boolean enabled) {
        PackageManager pm = context.getPackageManager();
        ComponentName component = getDeveloperOptionsModule(context);
        int state = pm.getComponentEnabledSetting(component);
        boolean isEnabled = isDeveloperOptionsModuleEnabled(context);
        LOG.i("Enabling developer options module: " + component.flattenToString()
                + " Current state: " + state
                + " Currently enabled: " + isEnabled
                + " Should enable: " + enabled);
        if (isEnabled != enabled || state == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT) {
            pm.setComponentEnabledSetting(component, enabled
                            ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                            : PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                    PackageManager.DONT_KILL_APP);
        }
    }
}
