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

import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.module.WallpaperPersister.WallpaperPosition;

/**
 * Test implementation of {@link UserEventLogger}.
 */
public class TestUserEventLogger implements UserEventLogger {

    private int mNumDailyRefreshTurnedOnEvents;
    private int mNumCurrentWallpaperPreviewedEvents;
    private int mNumActionClickedEvents;
    private int mNumIndividualWallpaperSelectedEvents;
    private int mNumCategorySelectedEvents;
    private int mNumWallpaperSetEvents;
    private int mNumWallpaperSetResultEvents;
    private String mLastCollectionId;
    private String mLastWallpaperId;
    @WallpaperSetResult
    private int mLastWallpaperSetResult;
    private int mLastDailyRotationHour;
    private int mNum1DayActiveLogs;
    private int mNum7DayActiveLogs;
    private int mNum14DayActiveLogs;
    private int mNum28DayActiveLogs;
    private int mLastDailyWallpaperRotationStatus;
    private int mNumDaysDailyRotationFailed;
    private int mNumDaysDailyRotationNotAttempted;
    private int mLastDailyWallpaperUpdateResult;
    private int mStandalonePreviewLaunches;
    private int mNumRestores;
    @WallpaperPosition
    private int mWallpaperPosition;

    public TestUserEventLogger() {
        mLastDailyRotationHour = -1;
        mLastDailyWallpaperRotationStatus = -1;
        mNumDaysDailyRotationFailed = -1;
        mNumDaysDailyRotationNotAttempted = -1;
    }

    @Override
    public void logResumed(boolean provisioned, boolean wallpaper) {

    }

    @Override
    public void logStopped() {

    }

    @Override
    public void logAppLaunched() {
        // Do nothing.
    }

    @Override
    public void logDailyRefreshTurnedOn() {
        mNumDailyRefreshTurnedOnEvents++;
    }

    public int getNumDailyRefreshTurnedOnEvents() {
        return mNumDailyRefreshTurnedOnEvents;
    }

    @Override
    public void logCurrentWallpaperPreviewed() {
        mNumCurrentWallpaperPreviewedEvents++;
    }

    @Override
    public void logActionClicked(String collectionId, int actionLabelResId) {
        mNumActionClickedEvents++;
        mLastCollectionId = collectionId;
    }

    public int getNumCurrentWallpaperPreviewedEvents() {
        return mNumCurrentWallpaperPreviewedEvents;
    }

    public int getNumActionClickedEvents() {
        return mNumActionClickedEvents;
    }

    @Override
    public void logIndividualWallpaperSelected(String collectionId) {
        mNumIndividualWallpaperSelectedEvents++;
        mLastCollectionId = collectionId;
    }

    public int getNumIndividualWallpaperSelectedEvents() {
        return mNumIndividualWallpaperSelectedEvents;
    }

    @Override
    public void logCategorySelected(String collectionId) {
        mNumCategorySelectedEvents++;
        mLastCollectionId = collectionId;
    }

    @Override
    public void logLiveWallpaperInfoSelected(String collectionId, String wallpaperId) {
        // No-op
    }

    @Override
    public void logLiveWallpaperCustomizeSelected(String collectionId, String wallpaperId) {
        // No-op
    }

    @Override
    public void logSnapshot() {
        // No-op
    }

    public int getNumCategorySelectedEvents() {
        return mNumCategorySelectedEvents;
    }

    @Override
    public void logWallpaperSet(String collectionId, String wallpaperId) {
        mNumWallpaperSetEvents++;
        mLastCollectionId = collectionId;
        mLastWallpaperId = wallpaperId;
    }

    @Override
    public void logWallpaperSetResult(@WallpaperSetResult int result) {
        mNumWallpaperSetResultEvents++;
        mLastWallpaperSetResult = result;
    }

    @Override
    public void logWallpaperSetFailureReason(@WallpaperSetFailureReason int reason) {
        // No-op
    }


    @Override
    public void logNumDailyWallpaperRotationsInLastWeek() {
        // No-op
    }

    @Override
    public void logNumDailyWallpaperRotationsPreviousDay() {
        // No-op
    }

    @Override
    public void logDailyWallpaperRotationHour(int hour) {
        mLastDailyRotationHour = hour;
    }

    @Override
    public void logDailyWallpaperDecodes(boolean decodes) {
        // No-op
    }

    @Override
    public void logRefreshDailyWallpaperButtonClicked() {
        // No-op
    }

    @Override
    public void logDailyWallpaperRotationStatus(int status) {
        mLastDailyWallpaperRotationStatus = status;
    }

    @Override
    public void logDailyWallpaperSetNextWallpaperResult(@DailyWallpaperUpdateResult int result) {
        mLastDailyWallpaperUpdateResult = result;
    }

    @Override
    public void logDailyWallpaperSetNextWallpaperCrash(@DailyWallpaperUpdateCrash int crash) {
        // No-op
    }

    @Override
    public void logNumDaysDailyRotationFailed(int days) {
        mNumDaysDailyRotationFailed = days;
    }

    @Override
    public void logDailyWallpaperMetadataRequestFailure(
            @DailyWallpaperMetadataFailureReason int reason) {
        // No-op
    }

    @Override
    public void logNumDaysDailyRotationNotAttempted(int days) {
        mNumDaysDailyRotationNotAttempted = days;
    }

    @Override
    public void logStandalonePreviewLaunched() {
        mStandalonePreviewLaunches++;
    }

    @Override
    public void logStandalonePreviewImageUriHasReadPermission(boolean isReadPermissionGranted) {
        // No-op
    }

    @Override
    public void logStandalonePreviewStorageDialogApproved(boolean isApproved) {
        // No-op
    }

    @Override
    public void logWallpaperPresentationMode() {
        // No-op
    }

    @Override
    public void logRestored() {
        mNumRestores++;
    }

    public int getNumWallpaperSetEvents() {
        return mNumWallpaperSetEvents;
    }

    public String getLastCollectionId() {
        return mLastCollectionId;
    }

    public String getLastWallpaperId() {
        return mLastWallpaperId;
    }

    public int getNumWallpaperSetResultEvents() {
        return mNumWallpaperSetResultEvents;
    }

    @WallpaperSetResult
    public int getLastWallpaperSetResult() {
        return mLastWallpaperSetResult;
    }

    public int getLastDailyRotationHour() {
        return mLastDailyRotationHour;
    }

    public int getNum1DayActiveLogs() {
        return mNum1DayActiveLogs;
    }

    public int getNum7DayActiveLogs() {
        return mNum7DayActiveLogs;
    }

    public int getNum14DayActiveLogs() {
        return mNum14DayActiveLogs;
    }

    public int getNum28DayActiveLogs() {
        return mNum28DayActiveLogs;
    }

    public int getLastDailyWallpaperRotationStatus() {
        return mLastDailyWallpaperRotationStatus;
    }

    public int getNumDaysDailyRotationFailed() {
        return mNumDaysDailyRotationFailed;
    }

    public int getNumDaysDailyRotationNotAttempted() {
        return mNumDaysDailyRotationNotAttempted;
    }

    public int getLastDailyWallpaperUpdateResult() {
        return mLastDailyWallpaperUpdateResult;
    }

    public int getStandalonePreviewLaunches() {
        return mStandalonePreviewLaunches;
    }

    public int getNumRestores() {
        return mNumRestores;
    }

    public int getWallpaperPosition() {
        return mWallpaperPosition;
    }
}
