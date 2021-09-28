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
package com.android.wallpaper.testing;

import static com.android.wallpaper.module.WallpaperPersister.DEST_BOTH;
import static com.android.wallpaper.module.WallpaperPersister.DEST_HOME_SCREEN;
import static com.android.wallpaper.module.WallpaperPersister.DEST_LOCK_SCREEN;

import androidx.annotation.Nullable;

import com.android.wallpaper.module.WallpaperPersister.Destination;
import com.android.wallpaper.module.WallpaperPreferences;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;

/**
 * Test implementation of the WallpaperPreferences interface. Just keeps prefs in memory.
 */
public class TestWallpaperPreferences implements WallpaperPreferences {

    private int mAppLaunchCount;
    private int mFirstLaunchDate;
    private int mFirstWallpaperApplyDate;
    @PresentationMode
    private int mWallpaperPresentationMode;

    private List<String> mHomeScreenAttributions;
    private long mHomeScreenBitmapHashCode;
    private int mHomeWallpaperManagerId;
    private String mHomeScreenPackageName;
    private String mHomeScreenServiceName;
    private String mHomeActionUrl;
    private String mHomeBaseImageUrl;
    private String mHomeCollectionId;
    private String mHomeWallpaperRemoteId;

    private List<String> mLockScreenAttributions;
    private long mLockScreenBitmapHashCode;
    private int mLockWallpaperManagerId;
    private String mLockActionUrl;
    private String mLockCollectionId;
    private String mLockWallpaperRemoteId;

    private List<Long> mDailyRotations;
    private long mDailyWallpaperEnabledTimestamp;
    private long mLastDailyLogTimestamp;
    private long mLastAppActiveTimestamp;
    private int mLastDailyWallpaperRotationStatus;
    private long mLastDailyWallpaperRotationStatusTimestamp;
    private long mLastSyncTimestamp;
    @PendingWallpaperSetStatus
    private int mPendingWallpaperSetStatus;
    @PendingDailyWallpaperUpdateStatus
    private int mPendingDailyWallpaperUpdateStatus;
    private int mNumDaysDailyRotationFailed;
    private int mNumDaysDailyRotationNotAttempted;
    private int mHomeWallpaperActionLabelRes;
    private int mHomeWallpaperActionIconRes;
    private int mLockWallpaperActionLabelRes;
    private int mLockWallpaperActionIconRes;

    public TestWallpaperPreferences() {
        mWallpaperPresentationMode = WallpaperPreferences.PRESENTATION_MODE_STATIC;
        mHomeScreenAttributions = Arrays.asList("Android wallpaper");
        mHomeScreenBitmapHashCode = 0;
        mHomeWallpaperManagerId = 0;

        mLockScreenAttributions = Arrays.asList("Android wallpaper");
        mLockScreenBitmapHashCode = 0;
        mLockWallpaperManagerId = 0;

        mDailyRotations = new ArrayList<>();
        mDailyWallpaperEnabledTimestamp = -1;
        mLastDailyLogTimestamp = -1;
        mLastDailyWallpaperRotationStatus = -1;
        mLastDailyWallpaperRotationStatusTimestamp = 0;
        mLastSyncTimestamp = 0;
        mPendingWallpaperSetStatus = WALLPAPER_SET_NOT_PENDING;
    }

    @Override
    public int getWallpaperPresentationMode() {
        return mWallpaperPresentationMode;
    }

    @Override
    public void setWallpaperPresentationMode(@PresentationMode int presentationMode) {
        mWallpaperPresentationMode = presentationMode;
    }

    @Override
    public List<String> getHomeWallpaperAttributions() {
        return mHomeScreenAttributions;
    }

    @Override
    public void setHomeWallpaperAttributions(List<String> attributions) {
        mHomeScreenAttributions = attributions;
    }

    @Override
    public String getHomeWallpaperActionUrl() {
        return mHomeActionUrl;
    }

    @Override
    public void setHomeWallpaperActionUrl(String actionUrl) {
        mHomeActionUrl = actionUrl;
    }

    @Override
    public int getHomeWallpaperActionLabelRes() {
        return mHomeWallpaperActionLabelRes;
    }

    @Override
    public void setHomeWallpaperActionLabelRes(int resId) {
        mHomeWallpaperActionLabelRes = resId;
    }

    @Override
    public int getHomeWallpaperActionIconRes() {
        return mHomeWallpaperActionIconRes;
    }

    @Override
    public void setHomeWallpaperActionIconRes(int resId) {
        mHomeWallpaperActionIconRes = resId;
    }

    @Override
    public String getHomeWallpaperBaseImageUrl() {
        return mHomeBaseImageUrl;
    }

    @Override
    public void setHomeWallpaperBaseImageUrl(String baseImageUrl) {
        mHomeBaseImageUrl = baseImageUrl;
    }

    @Override
    public String getHomeWallpaperCollectionId() {
        return mHomeCollectionId;
    }

    @Override
    public void setHomeWallpaperCollectionId(String collectionId) {
        mHomeCollectionId = collectionId;
    }

    @Override
    public String getHomeWallpaperBackingFileName() {
        return null;
    }

    @Override
    public void setHomeWallpaperBackingFileName(String fileName) {

    }

    @Override
    public void clearHomeWallpaperMetadata() {
        mHomeScreenAttributions = null;
        mWallpaperPresentationMode = WallpaperPreferences.PRESENTATION_MODE_STATIC;
        mHomeScreenBitmapHashCode = 0;
        mHomeScreenPackageName = null;
        mHomeWallpaperManagerId = 0;
    }

    @Override
    public long getHomeWallpaperHashCode() {
        return mHomeScreenBitmapHashCode;
    }

    @Override
    public void setHomeWallpaperHashCode(long hashCode) {
        mHomeScreenBitmapHashCode = hashCode;
    }

    @Override
    public String getHomeWallpaperPackageName() {
        return mHomeScreenPackageName;
    }

    @Override
    public void setHomeWallpaperPackageName(String packageName) {
        mHomeScreenPackageName = packageName;
    }

    @Override
    public String getHomeWallpaperServiceName() {
        return mHomeScreenServiceName;
    }

    @Override
    public void setHomeWallpaperServiceName(String serviceName) {
        mHomeScreenServiceName = serviceName;
        setFirstWallpaperApplyDateIfNeeded();
    }

    @Override
    public int getHomeWallpaperManagerId() {
        return mHomeWallpaperManagerId;
    }

    @Override
    public void setHomeWallpaperManagerId(int homeWallpaperId) {
        mHomeWallpaperManagerId = homeWallpaperId;
    }

    @Override
    public String getHomeWallpaperRemoteId() {
        return mHomeWallpaperRemoteId;
    }

    @Override
    public void setHomeWallpaperRemoteId(String wallpaperRemoteId) {
        mHomeWallpaperRemoteId = wallpaperRemoteId;
        setFirstWallpaperApplyDateIfNeeded();
    }

    @Override
    public List<String> getLockWallpaperAttributions() {
        return mLockScreenAttributions;
    }

    @Override
    public void setLockWallpaperAttributions(List<String> attributions) {
        mLockScreenAttributions = attributions;
    }

    @Override
    public String getLockWallpaperActionUrl() {
        return mLockActionUrl;
    }

    @Override
    public void setLockWallpaperActionUrl(String actionUrl) {
        mLockActionUrl = actionUrl;
    }

    @Override
    public int getLockWallpaperActionLabelRes() {
        return mLockWallpaperActionLabelRes;
    }

    @Override
    public void setLockWallpaperActionLabelRes(int resId) {
        mLockWallpaperActionLabelRes = resId;
    }

    @Override
    public int getLockWallpaperActionIconRes() {
        return mLockWallpaperActionIconRes;
    }

    @Override
    public void setLockWallpaperActionIconRes(int resId) {
        mLockWallpaperActionIconRes = resId;
    }

    @Override
    public String getLockWallpaperCollectionId() {
        return mLockCollectionId;
    }

    @Override
    public void setLockWallpaperCollectionId(String collectionId) {
        mLockCollectionId = collectionId;
    }

    @Override
    public String getLockWallpaperBackingFileName() {
        return null;
    }

    @Override
    public void setLockWallpaperBackingFileName(String fileName) {

    }

    @Override
    public void clearLockWallpaperMetadata() {
        mLockScreenAttributions = null;
        mLockScreenBitmapHashCode = 0;
        mLockWallpaperManagerId = 0;
    }

    @Override
    public long getLockWallpaperHashCode() {
        return mLockScreenBitmapHashCode;
    }

    @Override
    public void setLockWallpaperHashCode(long hashCode) {
        mLockScreenBitmapHashCode = hashCode;
    }

    @Override
    public int getLockWallpaperId() {
        return mLockWallpaperManagerId;
    }

    @Override
    public void setLockWallpaperId(int lockWallpaperId) {
        mLockWallpaperManagerId = lockWallpaperId;
    }

    @Override
    public String getLockWallpaperRemoteId() {
        return mLockWallpaperRemoteId;
    }

    @Override
    public void setLockWallpaperRemoteId(String wallpaperRemoteId) {
        mLockWallpaperRemoteId = wallpaperRemoteId;
        setFirstWallpaperApplyDateIfNeeded();
    }

    @Override
    public void addDailyRotation(long timestamp) {
        mDailyRotations.add(timestamp);
    }

    @Override
    public long getLastDailyRotationTimestamp() {
        if (mDailyRotations.size() == 0) {
            return -1;
        }

        return mDailyRotations.get(mDailyRotations.size() - 1);
    }

    @Override
    public List<Long> getDailyRotationsInLastWeek() {
        return mDailyRotations;
    }

    @Nullable
    @Override
    public List<Long> getDailyRotationsPreviousDay() {
        return null;
    }

    @Override
    public long getDailyWallpaperEnabledTimestamp() {
        return mDailyWallpaperEnabledTimestamp;
    }

    @Override
    public void setDailyWallpaperEnabledTimestamp(long timestamp) {
        mDailyWallpaperEnabledTimestamp = timestamp;
    }

    @Override
    public void clearDailyRotations() {
        mDailyRotations.clear();
    }

    @Override
    public long getLastDailyLogTimestamp() {
        return mLastDailyLogTimestamp;
    }

    @Override
    public void setLastDailyLogTimestamp(long timestamp) {
        mLastDailyLogTimestamp = timestamp;
    }

    @Override
    public long getLastAppActiveTimestamp() {
        return mLastAppActiveTimestamp;
    }

    @Override
    public void setLastAppActiveTimestamp(long timestamp) {
        mLastAppActiveTimestamp = timestamp;
    }

    @Override
    public void setDailyWallpaperRotationStatus(int status, long timestamp) {
        mLastDailyWallpaperRotationStatus = status;
        mLastDailyWallpaperRotationStatusTimestamp = timestamp;
    }

    @Override
    public int getDailyWallpaperLastRotationStatus() {
        return mLastDailyWallpaperRotationStatus;
    }

    @Override
    public long getDailyWallpaperLastRotationStatusTimestamp() {
        return mLastDailyWallpaperRotationStatusTimestamp;
    }

    @Override
    public long getLastSyncTimestamp() {
        return mLastSyncTimestamp;
    }

    @Override
    public void setLastSyncTimestamp(long timestamp) {
        mLastSyncTimestamp = timestamp;
    }

    @Override
    public void setPendingWallpaperSetStatusSync(@PendingWallpaperSetStatus int setStatus) {
        mPendingWallpaperSetStatus = setStatus;
    }

    @Override
    public int getPendingWallpaperSetStatus() {
        return mPendingWallpaperSetStatus;
    }

    @Override
    public void setPendingWallpaperSetStatus(@PendingWallpaperSetStatus int setStatus) {
        mPendingWallpaperSetStatus = setStatus;
    }

    @Override
    public void setPendingDailyWallpaperUpdateStatusSync(
            @PendingDailyWallpaperUpdateStatus int updateStatus) {
        mPendingDailyWallpaperUpdateStatus = updateStatus;
    }

    @Override
    public int getPendingDailyWallpaperUpdateStatus() {
        return mPendingDailyWallpaperUpdateStatus;
    }

    @Override
    public void setPendingDailyWallpaperUpdateStatus(
            @PendingDailyWallpaperUpdateStatus int updateStatus) {
        mPendingDailyWallpaperUpdateStatus = updateStatus;
    }

    @Override
    public void incrementNumDaysDailyRotationFailed() {
        mNumDaysDailyRotationFailed++;
    }

    @Override
    public int getNumDaysDailyRotationFailed() {
        return mNumDaysDailyRotationFailed;
    }

    public void setNumDaysDailyRotationFailed(int days) {
        mNumDaysDailyRotationFailed = days;
    }

    @Override
    public void resetNumDaysDailyRotationFailed() {
        mNumDaysDailyRotationFailed = 0;
    }

    @Override
    public void incrementNumDaysDailyRotationNotAttempted() {
        mNumDaysDailyRotationNotAttempted++;
    }

    @Override
    public int getNumDaysDailyRotationNotAttempted() {
        return mNumDaysDailyRotationNotAttempted;
    }

    public void setNumDaysDailyRotationNotAttempted(int days) {
        mNumDaysDailyRotationNotAttempted = days;
    }

    @Override
    public void resetNumDaysDailyRotationNotAttempted() {
        mNumDaysDailyRotationNotAttempted = 0;
    }


    @Override
    public int getAppLaunchCount() {
        return mAppLaunchCount;
    }

    private void setAppLaunchCount(int count) {
        mAppLaunchCount = count;
    }

    @Override
    public int getFirstLaunchDateSinceSetup() {
        return mFirstLaunchDate;
    }

    private void setFirstLaunchDateSinceSetup(int firstLaunchDate) {
        mFirstLaunchDate = firstLaunchDate;
    }

    @Override
    public int getFirstWallpaperApplyDateSinceSetup() {
        return mFirstWallpaperApplyDate;
    }

    private void setFirstWallpaperApplyDateSinceSetup(int firstWallpaperApplyDate) {
        mFirstWallpaperApplyDate = firstWallpaperApplyDate;
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
        if (destination == DEST_HOME_SCREEN) {
            setHomeWallpaperCollectionId(collectionId);
            setHomeWallpaperRemoteId(wallpaperId);
        } else if (destination == DEST_LOCK_SCREEN) {
            setLockWallpaperCollectionId(collectionId);
            setLockWallpaperRemoteId(wallpaperId);
        } else if (destination == DEST_BOTH) {
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
