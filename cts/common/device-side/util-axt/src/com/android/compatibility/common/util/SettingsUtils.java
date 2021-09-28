/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.compatibility.common.util;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;
import static com.google.common.truth.Truth.assertWithMessage;

import android.content.Context;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Provides utilities to interact with the device's {@link Settings}.
 */
public final class SettingsUtils {

    private static final String TAG = SettingsUtils.class.getSimpleName();

    public static final String NAMESPACE_SECURE = "secure";
    public static final String NAMESPACE_GLOBAL = "global";

    // TODO(b/123885378): we cannot pass an empty value when using 'cmd settings', so we need
    // to remove the property instead. Once we use the Settings API directly, we can remove this
    // constant and all if() statements that ues it
    static final boolean TMP_HACK_REMOVE_EMPTY_PROPERTIES = true;

    /**
     * Uses a Shell command to set the given preference.
     */
    public static void set(@NonNull String namespace, @NonNull String key, @Nullable String value) {
        if (value == null) {
            delete(namespace, key);
            return;
        }
        if (TMP_HACK_REMOVE_EMPTY_PROPERTIES && TextUtils.isEmpty(value)) {
            Log.w(TAG, "Value of " + namespace + ":" + key + " is empty; deleting it instead");
            delete(namespace, key);
            return;
        }
        runShellCommand("settings put %s %s %s default", namespace, key, value);
    }

    /**
     * Sets a preference in the {@link #NAMESPACE_SECURE} namespace.
     */
    public static void set(@NonNull String key, @Nullable String value) {
        set(NAMESPACE_SECURE, key, value);
    }

    /**
     * Uses a Shell command to set the given preference, and verifies it was correctly set.
     */
    public static void syncSet(@NonNull Context context, @NonNull String namespace,
            @NonNull String key, @Nullable String value) {
        if (value == null) {
            syncDelete(context, namespace, key);
            return;
        }

        final String currentValue = get(namespace, key);
        if (value.equals(currentValue)) {
            // Already set, ignore
            return;
        }

        final OneTimeSettingsListener observer =
                new OneTimeSettingsListener(context, namespace, key);
        set(namespace, key, value);
        observer.assertCalled();

        final String newValue = get(namespace, key);
        if (TMP_HACK_REMOVE_EMPTY_PROPERTIES && TextUtils.isEmpty(value)) {
            assertWithMessage("invalid value for '%s' settings", key).that(newValue)
                .isNull();
        } else {
            assertWithMessage("invalid value for '%s' settings", key).that(newValue)
                    .isEqualTo(value);
        }
    }

    /**
     * Sets a preference in the {@link #NAMESPACE_SECURE} namespace, using a Settings listener to
     * block until it's set.
     */
    public static void syncSet(@NonNull Context context, @NonNull String key,
            @Nullable String value) {
        syncSet(context, NAMESPACE_SECURE, key, value);
    }

    /**
     * Uses a Shell command to delete the given preference.
     */
    public static void delete(@NonNull String namespace, @NonNull String key) {
        runShellCommand("settings delete %s %s", namespace, key);
    }

    /**
     * Deletes a preference in the {@link #NAMESPACE_SECURE} namespace.
     */
    public static void delete(@NonNull String key) {
        delete(NAMESPACE_SECURE, key);
    }

    /**
     * Uses a Shell command to delete the given preference, and verifies it was correctly deleted.
     */
    public static void syncDelete(@NonNull Context context, @NonNull String namespace,
            @NonNull String key) {

        final String currentValue = get(namespace, key);
        if (currentValue == null) {
            // Already set, ignore
            return;
        }

        final OneTimeSettingsListener observer = new OneTimeSettingsListener(context, namespace,
                key);
        delete(namespace, key);
        observer.assertCalled();

        final String newValue = get(namespace, key);
        assertWithMessage("invalid value for '%s' settings", key).that(newValue).isNull();
    }

    /**
     * Deletes a preference in the {@link #NAMESPACE_SECURE} namespace, using a Settings listener to
     * block until it's deleted.
     */
    public static void syncDelete(@NonNull Context context, @NonNull String key) {
        syncDelete(context, NAMESPACE_SECURE, key);
    }

    /**
     * Gets the value of a given preference using Shell command.
     */
    @Nullable
    public static String get(@NonNull String namespace, @NonNull String key) {
        final String value = runShellCommand("settings get %s %s", namespace, key);
        if (value == null || value.equals("null")) {
            return null;
        } else {
            return value;
        }
    }

    /**
     * Gets the value of a preference in the {@link #NAMESPACE_SECURE} namespace.
     */
    @NonNull
    public static String get(@NonNull String key) {
        return get(NAMESPACE_SECURE, key);
    }

    private SettingsUtils() {
        throw new UnsupportedOperationException("contain static methods only");
    }

    /**
     * @deprecated - use {@link #set(String, String, String)} with {@link #NAMESPACE_GLOBAL}
     */
    @Deprecated
    public static void putGlobalSetting(String key, String value) {
        set(SettingsUtils.NAMESPACE_GLOBAL, key, value);

    }

    /**
     * @deprecated - use {@link #set(String, String, String)} with {@link #NAMESPACE_GLOBAL}
     */
    @Deprecated
    public static void putSecureSetting(String key, String value) {
        set(SettingsUtils.NAMESPACE_SECURE, key, value);

    }

    /**
     * Get a global setting for the current (foreground) user. Trims ending new line.
     */
    public static String getSecureSetting(String key) {
        return SystemUtil.runShellCommand("settings --user current get secure " + key).trim();
    }

    /**
     * Get a global setting for the given user. Trims ending new line.
     */
    public static String getSecureSettingAsUser(int userId, String key) {
        return SystemUtil.runShellCommand(
                String.format("settings --user %d get secure %s", userId, key)).trim();
    }
}
