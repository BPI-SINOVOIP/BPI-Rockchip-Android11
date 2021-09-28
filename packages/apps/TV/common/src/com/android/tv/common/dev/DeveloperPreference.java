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
 * limitations under the License
 */
package com.android.tv.common.dev;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

/** Preferences available to developers */
public abstract class DeveloperPreference<T> {

    private static final String PREFERENCE_FILE_NAME =
            "com.android.tv.common.dev.DeveloperPreference";

    /**
     * Create a boolean developer preference.
     *
     * @param key the developer setting key.
     * @param defaultValue the value to return if the setting is undefined or empty.
     */
    public static DeveloperPreference<Boolean> create(String key, boolean defaultValue) {
        return new DeveloperBooleanPreference(key, defaultValue);
    }

    @VisibleForTesting
    static final SharedPreferences getPreferences(Context context) {
        return context.getSharedPreferences(PREFERENCE_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Create a int developer preference.
     *
     * @param key the developer setting key.
     * @param defaultValue the value to return if the setting is undefined or empty.
     */
    public static DeveloperPreference<Integer> create(String key, int defaultValue) {
        return new DeveloperIntegerPreference(key, defaultValue);
    }

    final String mKey;
    final T mDefaultValue;
    private T mValue;

    private DeveloperPreference(String key, T defaultValue) {
        mKey = key;
        mValue = null;
        mDefaultValue = defaultValue;
    }

    /** Set the value. */
    public final void set(Context context, T value) {
        mValue = value;
        storeValue(context, value);
    }

    protected abstract void storeValue(Context context, T value);

    /** Get the current value, or the default if the value is not set. */
    public final T get(Context context) {
        mValue = getStoredValue(context);
        return mValue;
    }

    /** Get the current value, or the default if the value is not set or context is null. */
    public final T getDefaultIfContextNull(@Nullable Context context) {
        return context == null ? mDefaultValue : getStoredValue(context);
    }

    protected abstract T getStoredValue(Context context);

    /**
     * Clears the current value.
     *
     * <p>Future calls to {@link #get(Context)} will return the default value.
     */
    public final void clear(Context context) {
        getPreferences(context).edit().remove(mKey).apply();
    }

    @Override
    public final String toString() {
        return "[" + mKey + "]=" + mValue + " Default value : " + mDefaultValue;
    }

    private static final class DeveloperBooleanPreference extends DeveloperPreference<Boolean> {

        private DeveloperBooleanPreference(String key, Boolean defaultValue) {
            super(key, defaultValue);
        }

        @Override
        public void storeValue(Context context, Boolean value) {
            getPreferences(context).edit().putBoolean(mKey, value).apply();
        }

        @Override
        public Boolean getStoredValue(Context context) {
            return getPreferences(context).getBoolean(mKey, mDefaultValue);
        }
    }

    private static final class DeveloperIntegerPreference extends DeveloperPreference<Integer> {

        private DeveloperIntegerPreference(String key, Integer defaultValue) {
            super(key, defaultValue);
        }

        @Override
        protected void storeValue(Context context, Integer value) {
            getPreferences(context).edit().putInt(mKey, value).apply();
        }

        @Override
        protected Integer getStoredValue(Context context) {
            return getPreferences(context).getInt(mKey, mDefaultValue);
        }
    }
}
