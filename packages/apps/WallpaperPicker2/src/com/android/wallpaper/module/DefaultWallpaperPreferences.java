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
package com.android.wallpaper.module;

import android.app.backup.BackupManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.wallpaper.module.WallpaperPersister.Destination;
import com.android.wallpaper.module.WallpaperPreferenceKeys.NoBackupKeys;

import org.json.JSONArray;
import org.json.JSONException;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;

/**
 * Default implementation that writes to and reads from SharedPreferences.
 */
public class DefaultWallpaperPreferences implements WallpaperPreferences {
    public static final String PREFS_NAME = "wallpaper";
    public static final String NO_BACKUP_PREFS_NAME = "wallpaper-nobackup";

    private static final String TAG = "DefaultWPPreferences";

    protected SharedPreferences mSharedPrefs;
    protected SharedPreferences mNoBackupPrefs;
    protected Context mContext;

    // Keep a strong reference to this OnSharedPreferenceChangeListener to prevent the listener from
    // being garbage collected because SharedPreferences only holds a weak reference.
    private OnSharedPreferenceChangeListener mSharedPrefsChangedListener;

    public DefaultWallpaperPreferences(Context context) {
        mSharedPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        mNoBackupPrefs = context.getSharedPreferences(NO_BACKUP_PREFS_NAME, Context.MODE_PRIVATE);
        if (mNoBackupPrefs.getAll().isEmpty() && !mSharedPrefs.getAll().isEmpty()) {
            upgradePrefs();
        }
        mContext = context.getApplicationContext();

        // Register a prefs changed listener so that all prefs changes trigger a backup event.
        final BackupManager backupManager = new BackupManager(context);
        mSharedPrefsChangedListener = (sharedPreferences, key) -> backupManager.dataChanged();
        mSharedPrefs.registerOnSharedPreferenceChangeListener(mSharedPrefsChangedListener);
    }

    /**
     * Move {@link NoBackupKeys} preferences that might have been in mSharedPrefs from previous
     * versions of the app into mNoBackupPrefs.
     */
    private void upgradePrefs() {
        SharedPreferences.Editor editor = mNoBackupPrefs.edit();
        if (mSharedPrefs.contains(
                NoBackupKeys.KEY_HOME_WALLPAPER_BASE_IMAGE_URL)) {
            editor.putString(NoBackupKeys.KEY_HOME_WALLPAPER_BASE_IMAGE_URL,
                    mSharedPrefs.getString(NoBackupKeys.KEY_HOME_WALLPAPER_BASE_IMAGE_URL, null));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_HOME_WALLPAPER_MANAGER_ID)) {
            editor.putInt(NoBackupKeys.KEY_HOME_WALLPAPER_MANAGER_ID,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_HOME_WALLPAPER_MANAGER_ID, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_HOME_WALLPAPER_REMOTE_ID)) {
            editor.putString(NoBackupKeys.KEY_HOME_WALLPAPER_REMOTE_ID,
                    mSharedPrefs.getString(NoBackupKeys.KEY_HOME_WALLPAPER_REMOTE_ID, null));
        }
        if (mSharedPrefs.contains(
                NoBackupKeys.KEY_HOME_WALLPAPER_BACKING_FILE)) {
            editor.putString(NoBackupKeys.KEY_HOME_WALLPAPER_BACKING_FILE,
                    mSharedPrefs.getString(NoBackupKeys.KEY_HOME_WALLPAPER_BACKING_FILE, null));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_LOCK_WALLPAPER_MANAGER_ID)) {
            editor.putInt(NoBackupKeys.KEY_LOCK_WALLPAPER_MANAGER_ID,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_LOCK_WALLPAPER_MANAGER_ID, 0));
        }
        if (mSharedPrefs.contains(
                NoBackupKeys.KEY_LOCK_WALLPAPER_BACKING_FILE)) {
            editor.putString(NoBackupKeys.KEY_LOCK_WALLPAPER_BACKING_FILE,
                    mSharedPrefs.getString(NoBackupKeys.KEY_LOCK_WALLPAPER_BACKING_FILE, null));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS)) {
            editor.putString(NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS,
                    mSharedPrefs.getString(NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS, null));
        }
        if (mSharedPrefs.contains(
                NoBackupKeys.KEY_DAILY_WALLPAPER_ENABLED_TIMESTAMP)) {
            editor.putLong(NoBackupKeys.KEY_DAILY_WALLPAPER_ENABLED_TIMESTAMP,
                    mSharedPrefs.getLong(NoBackupKeys.KEY_DAILY_WALLPAPER_ENABLED_TIMESTAMP, -1));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_LAST_DAILY_LOG_TIMESTAMP)) {
            editor.putLong(NoBackupKeys.KEY_LAST_DAILY_LOG_TIMESTAMP,
                    mSharedPrefs.getLong(NoBackupKeys.KEY_LAST_DAILY_LOG_TIMESTAMP, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_LAST_APP_ACTIVE_TIMESTAMP)) {
            editor.putLong(NoBackupKeys.KEY_LAST_APP_ACTIVE_TIMESTAMP,
                    mSharedPrefs.getLong(NoBackupKeys.KEY_LAST_APP_ACTIVE_TIMESTAMP, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_LAST_ROTATION_STATUS)) {
            editor.putInt(NoBackupKeys.KEY_LAST_ROTATION_STATUS,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_LAST_ROTATION_STATUS, -1));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_LAST_ROTATION_STATUS_TIMESTAMP)) {
            editor.putLong(NoBackupKeys.KEY_LAST_ROTATION_STATUS_TIMESTAMP,
                    mSharedPrefs.getLong(NoBackupKeys.KEY_LAST_ROTATION_STATUS_TIMESTAMP, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_LAST_SYNC_TIMESTAMP)) {
            editor.putLong(NoBackupKeys.KEY_LAST_SYNC_TIMESTAMP,
                    mSharedPrefs.getLong(NoBackupKeys.KEY_LAST_SYNC_TIMESTAMP, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_PENDING_WALLPAPER_SET_STATUS)) {
            editor.putInt(NoBackupKeys.KEY_PENDING_WALLPAPER_SET_STATUS,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_PENDING_WALLPAPER_SET_STATUS,
                            WALLPAPER_SET_NOT_PENDING));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_PENDING_DAILY_WALLPAPER_UPDATE_STATUS)) {
            editor.putInt(NoBackupKeys.KEY_PENDING_DAILY_WALLPAPER_UPDATE_STATUS,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_PENDING_DAILY_WALLPAPER_UPDATE_STATUS,
                            DAILY_WALLPAPER_UPDATE_NOT_PENDING));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_FAILED)) {
            editor.putInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_FAILED,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_FAILED, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_NOT_ATTEMPTED)) {
            editor.putInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_NOT_ATTEMPTED,
                    mSharedPrefs.getInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_NOT_ATTEMPTED, 0));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_HOME_WALLPAPER_PACKAGE_NAME)) {
            editor.putString(NoBackupKeys.KEY_HOME_WALLPAPER_PACKAGE_NAME,
                    mSharedPrefs.getString(NoBackupKeys.KEY_HOME_WALLPAPER_PACKAGE_NAME, null));
        }
        if (mSharedPrefs.contains(NoBackupKeys.KEY_HOME_WALLPAPER_SERVICE_NAME)) {
            editor.putString(NoBackupKeys.KEY_HOME_WALLPAPER_SERVICE_NAME,
                    mSharedPrefs.getString(NoBackupKeys.KEY_HOME_WALLPAPER_SERVICE_NAME, null));
        }

        editor.apply();
    }

    private int getResIdPersistedByName(String key, String type) {
        String resName = mSharedPrefs.getString(key, null);
        if (resName == null) {
            return 0;
        }
        return mContext.getResources().getIdentifier(resName, type,
                mContext.getPackageName());
    }

    private void persistResIdByName(String key, int resId) {
        String resName = mContext.getResources().getResourceName(resId);
        mSharedPrefs.edit().putString(key, resName).apply();
    }

    @Override
    public int getWallpaperPresentationMode() {
        @PresentationMode
        int homeWallpaperPresentationMode = mSharedPrefs.getInt(
                WallpaperPreferenceKeys.KEY_WALLPAPER_PRESENTATION_MODE,
                WallpaperPreferences.PRESENTATION_MODE_STATIC);
        return homeWallpaperPresentationMode;
    }

    @Override
    public void setWallpaperPresentationMode(@PresentationMode int presentationMode) {
        mSharedPrefs.edit().putInt(
                WallpaperPreferenceKeys.KEY_WALLPAPER_PRESENTATION_MODE, presentationMode).apply();
    }

    @Override
    public List<String> getHomeWallpaperAttributions() {
        return Arrays.asList(
                mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_1, null),
                mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_2, null),
                mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_3, null));

    }

    @Override
    public void setHomeWallpaperAttributions(List<String> attributions) {
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        if (attributions.size() > 0) {
            editor.putString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_1,
                    attributions.get(0));
        }
        if (attributions.size() > 1) {
            editor.putString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_2,
                    attributions.get(1));
        }
        if (attributions.size() > 2) {
            editor.putString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_3,
                    attributions.get(2));
        }
        editor.apply();
    }

    @Override
    @Nullable
    public String getHomeWallpaperActionUrl() {
        return mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_URL, null);
    }

    @Override
    public void setHomeWallpaperActionUrl(String actionUrl) {
        mSharedPrefs.edit().putString(
                WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_URL, actionUrl).apply();
    }

    @Override
    public int getHomeWallpaperActionLabelRes() {
        // We need to store and read the resource names as their ids could change from build to
        // build and we might end up reading the wrong id
        return getResIdPersistedByName(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_LABEL_RES,
                "string");
    }

    @Override
    public void setHomeWallpaperActionLabelRes(int resId) {
        persistResIdByName(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_LABEL_RES, resId);
    }

    @Override
    public int getHomeWallpaperActionIconRes() {
        return getResIdPersistedByName(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_ICON_RES,
                "drawable");
    }

    @Override
    public void setHomeWallpaperActionIconRes(int resId) {
        persistResIdByName(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_ICON_RES, resId);
    }

    @Override
    public String getHomeWallpaperBaseImageUrl() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_HOME_WALLPAPER_BASE_IMAGE_URL, null);
    }

    @Override
    public void setHomeWallpaperBaseImageUrl(String baseImageUrl) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_HOME_WALLPAPER_BASE_IMAGE_URL, baseImageUrl)
                .apply();
    }

    @Override
    @Nullable
    public String getHomeWallpaperCollectionId() {
        return mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_COLLECTION_ID,
                null);
    }

    @Override
    public void setHomeWallpaperCollectionId(String collectionId) {
        mSharedPrefs.edit().putString(
                WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_COLLECTION_ID, collectionId).apply();
    }

    @Override
    @Nullable
    public String getHomeWallpaperBackingFileName() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_HOME_WALLPAPER_BACKING_FILE, null);
    }

    @Override
    public void setHomeWallpaperBackingFileName(String fileName) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_HOME_WALLPAPER_BACKING_FILE, fileName).apply();
    }

    @Override
    public long getHomeWallpaperHashCode() {
        return mSharedPrefs.getLong(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_HASH_CODE, 0);
    }

    @Override
    public void setHomeWallpaperHashCode(long hashCode) {
        mSharedPrefs.edit().putLong(
                WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_HASH_CODE, hashCode).apply();
    }

    @Override
    public void clearHomeWallpaperMetadata() {
        String homeWallpaperBackingFileName = getHomeWallpaperBackingFileName();
        if (!TextUtils.isEmpty(homeWallpaperBackingFileName)) {
            new File(homeWallpaperBackingFileName).delete();
        }
        mSharedPrefs.edit()
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_1)
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_2)
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ATTRIB_3)
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_URL)
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_LABEL_RES)
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_ACTION_ICON_RES)
                .remove(WallpaperPreferenceKeys.KEY_HOME_WALLPAPER_HASH_CODE)
                .apply();

        mNoBackupPrefs.edit()
                .remove(NoBackupKeys.KEY_HOME_WALLPAPER_PACKAGE_NAME)
                .remove(NoBackupKeys.KEY_HOME_WALLPAPER_MANAGER_ID)
                .remove(NoBackupKeys.KEY_HOME_WALLPAPER_REMOTE_ID)
                .remove(NoBackupKeys.KEY_HOME_WALLPAPER_SERVICE_NAME)
                .remove(NoBackupKeys.KEY_HOME_WALLPAPER_BASE_IMAGE_URL)
                .remove(NoBackupKeys.KEY_HOME_WALLPAPER_BACKING_FILE)
                .apply();
    }

    @Override
    public String getHomeWallpaperPackageName() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_HOME_WALLPAPER_PACKAGE_NAME, null);
    }

    @Override
    public void setHomeWallpaperPackageName(String packageName) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_HOME_WALLPAPER_PACKAGE_NAME, packageName)
                .apply();
    }

    @Override
    public String getHomeWallpaperServiceName() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_HOME_WALLPAPER_SERVICE_NAME, null);
    }

    @Override
    public void setHomeWallpaperServiceName(@NonNull String serviceName) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_HOME_WALLPAPER_SERVICE_NAME, serviceName)
                .apply();
        setFirstWallpaperApplyDateIfNeeded();
    }

    @Override
    public int getHomeWallpaperManagerId() {
        return mNoBackupPrefs.getInt(
                NoBackupKeys.KEY_HOME_WALLPAPER_MANAGER_ID, 0);
    }

    @Override
    public void setHomeWallpaperManagerId(int homeWallpaperId) {
        mNoBackupPrefs.edit().putInt(
                NoBackupKeys.KEY_HOME_WALLPAPER_MANAGER_ID, homeWallpaperId)
                .apply();
    }

    @Nullable
    @Override
    public String getHomeWallpaperRemoteId() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_HOME_WALLPAPER_REMOTE_ID, null);
    }

    @Override
    public void setHomeWallpaperRemoteId(@Nullable String wallpaperRemoteId) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_HOME_WALLPAPER_REMOTE_ID, wallpaperRemoteId)
                .apply();
        setFirstWallpaperApplyDateIfNeeded();
    }

    @Override
    public List<String> getLockWallpaperAttributions() {
        return Arrays.asList(
                mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_1, null),
                mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_2, null),
                mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_3, null));

    }

    @Override
    public void setLockWallpaperAttributions(List<String> attributions) {
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        if (attributions.size() > 0) {
            editor.putString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_1,
                    attributions.get(0));
        }
        if (attributions.size() > 1) {
            editor.putString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_2,
                    attributions.get(1));
        }
        if (attributions.size() > 2) {
            editor.putString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_3,
                    attributions.get(2));
        }
        editor.apply();
    }

    @Override
    @Nullable
    public String getLockWallpaperActionUrl() {
        return mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_URL, null);
    }

    @Override
    public void setLockWallpaperActionUrl(String actionUrl) {
        mSharedPrefs.edit().putString(
                WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_URL, actionUrl).apply();
    }

    @Override
    public int getLockWallpaperActionLabelRes() {
        // We need to store and read the resource names as their ids could change from build to
        // build and we might end up reading the wrong id
        return getResIdPersistedByName(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_LABEL_RES,
                "string");
    }

    @Override
    public void setLockWallpaperActionLabelRes(int resId) {
        persistResIdByName(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_LABEL_RES, resId);
    }

    @Override
    public int getLockWallpaperActionIconRes() {
        return getResIdPersistedByName(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_ICON_RES,
                "drawable");
    }

    @Override
    public void setLockWallpaperActionIconRes(int resId) {
        persistResIdByName(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_ICON_RES, resId);
    }

    @Override
    @Nullable
    public String getLockWallpaperCollectionId() {
        return mSharedPrefs.getString(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_COLLECTION_ID,
                null);
    }

    @Override
    public void setLockWallpaperCollectionId(String collectionId) {
        mSharedPrefs.edit().putString(
                WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_COLLECTION_ID, collectionId).apply();
    }

    @Override
    @Nullable
    public String getLockWallpaperBackingFileName() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_LOCK_WALLPAPER_BACKING_FILE, null);
    }

    @Override
    public void setLockWallpaperBackingFileName(String fileName) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_LOCK_WALLPAPER_BACKING_FILE, fileName).apply();
    }

    @Override
    public int getLockWallpaperId() {
        return mNoBackupPrefs.getInt(
                NoBackupKeys.KEY_LOCK_WALLPAPER_MANAGER_ID, 0);
    }

    @Override
    public void setLockWallpaperId(int lockWallpaperId) {
        mNoBackupPrefs.edit().putInt(
                NoBackupKeys.KEY_LOCK_WALLPAPER_MANAGER_ID, lockWallpaperId)
                .apply();
    }

    @Override
    public String getLockWallpaperRemoteId() {
        return mNoBackupPrefs.getString(
                NoBackupKeys.KEY_LOCK_WALLPAPER_REMOTE_ID, null);
    }

    @Override
    public void setLockWallpaperRemoteId(String wallpaperRemoteId) {
        mNoBackupPrefs.edit().putString(
                NoBackupKeys.KEY_LOCK_WALLPAPER_REMOTE_ID, wallpaperRemoteId)
                .apply();
        setFirstWallpaperApplyDateIfNeeded();
    }

    @Override
    public long getLockWallpaperHashCode() {
        return mSharedPrefs.getLong(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_HASH_CODE, 0);
    }

    @Override
    public void setLockWallpaperHashCode(long hashCode) {
        mSharedPrefs.edit()
                .putLong(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_HASH_CODE, hashCode)
                .apply();
    }

    @Override
    public void clearLockWallpaperMetadata() {
        String lockWallpaperBackingFileName = getLockWallpaperBackingFileName();
        if (!TextUtils.isEmpty(lockWallpaperBackingFileName)) {
            new File(lockWallpaperBackingFileName).delete();
        }
        mSharedPrefs.edit()
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_1)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_2)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ATTRIB_3)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_URL)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_LABEL_RES)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_ACTION_ICON_RES)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_COLLECTION_ID)
                .remove(WallpaperPreferenceKeys.KEY_LOCK_WALLPAPER_HASH_CODE)
                .apply();

        mNoBackupPrefs.edit()
                .remove(NoBackupKeys.KEY_LOCK_WALLPAPER_MANAGER_ID)
                .remove(NoBackupKeys.KEY_LOCK_WALLPAPER_BACKING_FILE)
                .remove(NoBackupKeys.KEY_LOCK_WALLPAPER_REMOTE_ID)
                .apply();
    }

    @Override
    public void addDailyRotation(long timestamp) {
        String jsonString = mNoBackupPrefs.getString(
                NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS, "[]");
        try {
            JSONArray jsonArray = new JSONArray(jsonString);
            jsonArray.put(timestamp);

            mNoBackupPrefs.edit()
                    .putString(NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS,
                            jsonArray.toString())
                    .apply();
        } catch (JSONException e) {
            Log.e(TAG, "Failed to add a daily rotation timestamp due to a JSON parse exception");
        }
    }

    @Override
    public long getLastDailyRotationTimestamp() {
        String jsonString = mNoBackupPrefs.getString(
                NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS, "[]");

        try {
            JSONArray jsonArray = new JSONArray(jsonString);

            if (jsonArray.length() == 0) {
                return -1;
            }

            return jsonArray.getLong(jsonArray.length() - 1);
        } catch (JSONException e) {
            Log.e(TAG, "Failed to find a daily rotation timestamp due to a JSON parse exception");
            return -1;
        }
    }

    @Override
    @Nullable
    public List<Long> getDailyRotationsInLastWeek() {
        long enabledTimestamp = getDailyWallpaperEnabledTimestamp();

        Calendar oneWeekAgo = Calendar.getInstance();
        oneWeekAgo.setTime(new Date());
        oneWeekAgo.add(Calendar.WEEK_OF_YEAR, -1);
        long oneWeekAgoTimestamp = oneWeekAgo.getTimeInMillis();

        // Return null if daily rotation wasn't enabled (timestamp value of -1) or was enabled
        // less than one week ago.
        if (enabledTimestamp == -1 || enabledTimestamp > oneWeekAgoTimestamp) {
            return null;
        }

        List<Long> timestamps = new ArrayList<>();
        String jsonString = mNoBackupPrefs.getString(
                NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS, "[]");

        try {
            JSONArray jsonArray = new JSONArray(jsonString);

            // Before recording the new daily rotation timestamp, filter out any that are older than
            // 1 week old.
            for (int i = 0; i < jsonArray.length(); i++) {
                long existingTimestamp = jsonArray.getLong(i);
                if (existingTimestamp >= oneWeekAgoTimestamp) {
                    timestamps.add(existingTimestamp);
                }
            }

            jsonArray = new JSONArray(timestamps);
            mNoBackupPrefs.edit()
                    .putString(NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS,
                            jsonArray.toString())
                    .apply();
        } catch (JSONException e) {
            Log.e(TAG, "Failed to get daily rotation timestamps due to a JSON parse exception");
        }

        return timestamps;
    }

    @Nullable
    @Override
    public List<Long> getDailyRotationsPreviousDay() {
        long enabledTimestamp = getDailyWallpaperEnabledTimestamp();

        Calendar midnightYesterday = Calendar.getInstance();
        midnightYesterday.set(Calendar.AM_PM, Calendar.AM);
        midnightYesterday.set(Calendar.HOUR, 0);
        midnightYesterday.set(Calendar.MINUTE, 0);
        midnightYesterday.add(Calendar.DATE, -1);
        long midnightYesterdayTimestamp = midnightYesterday.getTimeInMillis();

        Calendar midnightToday = Calendar.getInstance();
        midnightToday.set(Calendar.AM_PM, Calendar.AM);
        midnightToday.set(Calendar.HOUR, 0);
        midnightToday.set(Calendar.MINUTE, 0);
        long midnightTodayTimestamp = midnightToday.getTimeInMillis();

        // Return null if daily rotation wasn't enabled (timestamp value of -1) or was enabled
        // less than midnight yesterday.
        if (enabledTimestamp == -1 || enabledTimestamp > midnightYesterdayTimestamp) {
            return null;
        }

        List<Long> timestamps = new ArrayList<>();
        String jsonString = mNoBackupPrefs.getString(
                NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS, "[]");

        try {
            JSONArray jsonArray = new JSONArray(jsonString);

            // Filter the timestamps (which cover up to one week of data) to only include those
            // between midnight yesterday and midnight today.
            for (int i = 0; i < jsonArray.length(); i++) {
                long timestamp = jsonArray.getLong(i);
                if (timestamp >= midnightYesterdayTimestamp && timestamp < midnightTodayTimestamp) {
                    timestamps.add(timestamp);
                }
            }

        } catch (JSONException e) {
            Log.e(TAG, "Failed to get daily rotation timestamps due to a JSON parse exception");
        }

        return timestamps;
    }

    @Override
    public long getDailyWallpaperEnabledTimestamp() {
        return mNoBackupPrefs.getLong(
                NoBackupKeys.KEY_DAILY_WALLPAPER_ENABLED_TIMESTAMP, -1);
    }

    @Override
    public void setDailyWallpaperEnabledTimestamp(long timestamp) {
        mNoBackupPrefs.edit()
                .putLong(NoBackupKeys.KEY_DAILY_WALLPAPER_ENABLED_TIMESTAMP,
                        timestamp)
                .apply();
    }

    @Override
    public void clearDailyRotations() {
        mNoBackupPrefs.edit()
                .remove(NoBackupKeys.KEY_DAILY_ROTATION_TIMESTAMPS)
                .remove(NoBackupKeys.KEY_DAILY_WALLPAPER_ENABLED_TIMESTAMP)
                .apply();
    }

    @Override
    public long getLastDailyLogTimestamp() {
        return mNoBackupPrefs.getLong(
                NoBackupKeys.KEY_LAST_DAILY_LOG_TIMESTAMP, 0);
    }

    @Override
    public void setLastDailyLogTimestamp(long timestamp) {
        mNoBackupPrefs.edit()
                .putLong(NoBackupKeys.KEY_LAST_DAILY_LOG_TIMESTAMP, timestamp)
                .apply();
    }

    @Override
    public long getLastAppActiveTimestamp() {
        return mNoBackupPrefs.getLong(
                NoBackupKeys.KEY_LAST_APP_ACTIVE_TIMESTAMP, 0);
    }

    @Override
    public void setLastAppActiveTimestamp(long timestamp) {
        mNoBackupPrefs.edit()
                .putLong(NoBackupKeys.KEY_LAST_APP_ACTIVE_TIMESTAMP, timestamp)
                .apply();
    }

    @Override
    public void setDailyWallpaperRotationStatus(int status, long timestamp) {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_LAST_ROTATION_STATUS, status)
                .putLong(NoBackupKeys.KEY_LAST_ROTATION_STATUS_TIMESTAMP,
                        timestamp)
                .apply();
    }

    @Override
    public int getDailyWallpaperLastRotationStatus() {
        return mNoBackupPrefs.getInt(NoBackupKeys.KEY_LAST_ROTATION_STATUS, -1);
    }

    @Override
    public long getDailyWallpaperLastRotationStatusTimestamp() {
        return mNoBackupPrefs.getLong(
                NoBackupKeys.KEY_LAST_ROTATION_STATUS_TIMESTAMP, 0);
    }

    @Override
    public long getLastSyncTimestamp() {
        return mNoBackupPrefs.getLong(NoBackupKeys.KEY_LAST_SYNC_TIMESTAMP, 0);
    }

    @Override
    public void setLastSyncTimestamp(long timestamp) {
        // Write synchronously via commit() to ensure this timetsamp gets written to disk
        // immediately.
        mNoBackupPrefs.edit()
                .putLong(NoBackupKeys.KEY_LAST_SYNC_TIMESTAMP, timestamp)
                .commit();
    }

    @Override
    public void setPendingWallpaperSetStatusSync(@PendingWallpaperSetStatus int setStatus) {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_PENDING_WALLPAPER_SET_STATUS,
                        setStatus)
                .commit();
    }

    @Override
    public int getPendingWallpaperSetStatus() {
        //noinspection ResourceType
        return mNoBackupPrefs.getInt(
                NoBackupKeys.KEY_PENDING_WALLPAPER_SET_STATUS,
                WALLPAPER_SET_NOT_PENDING);
    }

    @Override
    public void setPendingWallpaperSetStatus(@PendingWallpaperSetStatus int setStatus) {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_PENDING_WALLPAPER_SET_STATUS,
                        setStatus)
                .apply();
    }

    @Override
    public void setPendingDailyWallpaperUpdateStatusSync(
            @PendingDailyWallpaperUpdateStatus int updateStatus) {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_PENDING_DAILY_WALLPAPER_UPDATE_STATUS,
                        updateStatus)
                .commit();
    }

    @Override
    public int getPendingDailyWallpaperUpdateStatus() {
        //noinspection ResourceType
        return mNoBackupPrefs.getInt(
                NoBackupKeys.KEY_PENDING_DAILY_WALLPAPER_UPDATE_STATUS,
                DAILY_WALLPAPER_UPDATE_NOT_PENDING);
    }

    @Override
    public void setPendingDailyWallpaperUpdateStatus(
            @PendingDailyWallpaperUpdateStatus int updateStatus) {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_PENDING_DAILY_WALLPAPER_UPDATE_STATUS,
                        updateStatus)
                .apply();
    }

    @Override
    public void incrementNumDaysDailyRotationFailed() {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_FAILED,
                        getNumDaysDailyRotationFailed() + 1)
                .apply();
    }

    @Override
    public int getNumDaysDailyRotationFailed() {
        return mNoBackupPrefs.getInt(
                NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_FAILED, 0);
    }

    @Override
    public void resetNumDaysDailyRotationFailed() {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_FAILED, 0)
                .apply();
    }

    @Override
    public void incrementNumDaysDailyRotationNotAttempted() {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_NOT_ATTEMPTED,
                        getNumDaysDailyRotationNotAttempted() + 1)
                .apply();
    }

    @Override
    public int getNumDaysDailyRotationNotAttempted() {
        return mNoBackupPrefs.getInt(
                NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_NOT_ATTEMPTED, 0);
    }

    @Override
    public void resetNumDaysDailyRotationNotAttempted() {
        mNoBackupPrefs.edit()
                .putInt(NoBackupKeys.KEY_NUM_DAYS_DAILY_ROTATION_NOT_ATTEMPTED, 0)
                .apply();
    }

    @Override
    public int getAppLaunchCount() {
        return mNoBackupPrefs.getInt(NoBackupKeys.KEY_APP_LAUNCH_COUNT, 0);
    }

    private void setAppLaunchCount(int count) {
        mNoBackupPrefs.edit().putInt(NoBackupKeys.KEY_APP_LAUNCH_COUNT, count).apply();
    }

    @Override
    public int getFirstLaunchDateSinceSetup() {
        return mNoBackupPrefs.getInt(NoBackupKeys.KEY_FIRST_LAUNCH_DATE_SINCE_SETUP, 0);
    }

    private void setFirstLaunchDateSinceSetup(int firstLaunchDate) {
        mNoBackupPrefs.edit().putInt(NoBackupKeys.KEY_FIRST_LAUNCH_DATE_SINCE_SETUP,
                firstLaunchDate).apply();
    }

    @Override
    public int getFirstWallpaperApplyDateSinceSetup() {
        return mNoBackupPrefs.getInt(NoBackupKeys.KEY_FIRST_WALLPAPER_APPLY_DATE_SINCE_SETUP, 0);
    }

    private void setFirstWallpaperApplyDateSinceSetup(int firstApplyDate) {
        mNoBackupPrefs.edit().putInt(NoBackupKeys.KEY_FIRST_WALLPAPER_APPLY_DATE_SINCE_SETUP,
                firstApplyDate).apply();
    }

    @Override
    public void incrementAppLaunched() {
        if (getFirstLaunchDateSinceSetup() == 0) {
            setFirstLaunchDateSinceSetup(getCurrentDate());
        }

        int appLaunchCount = getAppLaunchCount();
        if (appLaunchCount < Integer.MAX_VALUE) {
            setAppLaunchCount(appLaunchCount + 1);
        }
    }

    private void setFirstWallpaperApplyDateIfNeeded() {
        if (getFirstWallpaperApplyDateSinceSetup() == 0) {
            setFirstWallpaperApplyDateSinceSetup(getCurrentDate());
        }
    }

    @Override
    public void updateDailyWallpaperSet(@Destination int destination, String collectionId,
            String wallpaperId) {
        // Assign wallpaper info by destination.
        if (destination == WallpaperPersister.DEST_HOME_SCREEN) {
            setHomeWallpaperCollectionId(collectionId);
            setHomeWallpaperRemoteId(wallpaperId);
        } else if (destination == WallpaperPersister.DEST_LOCK_SCREEN) {
            setLockWallpaperCollectionId(collectionId);
            setLockWallpaperRemoteId(wallpaperId);
        } else if (destination == WallpaperPersister.DEST_BOTH) {
            setHomeWallpaperCollectionId(collectionId);
            setHomeWallpaperRemoteId(wallpaperId);
            setLockWallpaperCollectionId(collectionId);
            setLockWallpaperRemoteId(wallpaperId);
        }
    }

    private int getCurrentDate() {
        Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        SimpleDateFormat format = new SimpleDateFormat("yyyyMMdd", Locale.US);
        return Integer.parseInt(format.format(calendar.getTime()));
    }
}
