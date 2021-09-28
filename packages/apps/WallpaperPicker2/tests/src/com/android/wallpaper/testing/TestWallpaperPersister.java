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

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.Nullable;

import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.Asset.BitmapReceiver;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.WallpaperChangedNotifier;
import com.android.wallpaper.module.WallpaperPersister;
import com.android.wallpaper.module.WallpaperPreferences;

import java.util.List;

/**
 * Test double for {@link WallpaperPersister}.
 */
public class TestWallpaperPersister implements WallpaperPersister {

    private Context mAppContext;
    private WallpaperPreferences mPrefs;
    private WallpaperChangedNotifier mWallpaperChangedNotifier;
    private Bitmap mCurrentHomeWallpaper;
    private Bitmap mCurrentLockWallpaper;
    private Bitmap mPendingHomeWallpaper;
    private Bitmap mPendingLockWallpaper;
    private List<String> mHomeAttributions;
    private String mHomeActionUrl;
    @Destination
    private int mDestination;
    private WallpaperPersister.SetWallpaperCallback mCallback;
    private boolean mFailNextCall;
    private Rect mCropRect;
    private float mScale;
    @WallpaperPosition
    private int mWallpaperPosition;
    private WallpaperInfo mWallpaperInfo;

    public TestWallpaperPersister(Context appContext) {
        mAppContext = appContext;
        mPrefs = InjectorProvider.getInjector().getPreferences(appContext);
        mWallpaperChangedNotifier = WallpaperChangedNotifier.getInstance();

        mCurrentHomeWallpaper = null;
        mCurrentLockWallpaper = null;
        mPendingHomeWallpaper = null;
        mPendingLockWallpaper = null;
        mWallpaperInfo = null;
        mFailNextCall = false;
        mScale = -1.0f;
    }

    @Override
    public void setIndividualWallpaper(final WallpaperInfo wallpaperInfo, Asset asset,
            @Nullable final Rect cropRect, final float scale, final @Destination int destination,
            final WallpaperPersister.SetWallpaperCallback callback) {
        asset.decodeBitmap(50, 50, bitmap -> {
            if (destination == DEST_HOME_SCREEN || destination == DEST_BOTH) {
                mPendingHomeWallpaper = bitmap;
                mPrefs.setHomeWallpaperAttributions(wallpaperInfo.getAttributions(mAppContext));
                mPrefs.setWallpaperPresentationMode(
                        WallpaperPreferences.PRESENTATION_MODE_STATIC);
                mPrefs.setHomeWallpaperRemoteId(wallpaperInfo.getWallpaperId());
            }
            if (destination == DEST_LOCK_SCREEN || destination == DEST_BOTH) {
                mPendingLockWallpaper = bitmap;
                mPrefs.setLockWallpaperAttributions(wallpaperInfo.getAttributions(mAppContext));
                mPrefs.setLockWallpaperRemoteId(wallpaperInfo.getWallpaperId());
            }
            mDestination = destination;
            mCallback = callback;
            mCropRect = cropRect;
            mScale = scale;
            mWallpaperInfo = wallpaperInfo;
        });
    }

    @Override
    public void setIndividualWallpaperWithPosition(Activity activity, WallpaperInfo wallpaper,
            @WallpaperPosition int wallpaperPosition, SetWallpaperCallback callback) {
        wallpaper.getAsset(activity).decodeBitmap(50, 50, new BitmapReceiver() {
            @Override
            public void onBitmapDecoded(@Nullable Bitmap bitmap) {
                mPendingHomeWallpaper = bitmap;
                mPrefs.setHomeWallpaperAttributions(wallpaper.getAttributions(mAppContext));
                mPrefs.setHomeWallpaperBaseImageUrl(wallpaper.getBaseImageUrl());
                mPrefs.setHomeWallpaperActionUrl(wallpaper.getActionUrl(mAppContext));
                mPrefs.setHomeWallpaperCollectionId(wallpaper.getCollectionId(mAppContext));
                mPrefs.setHomeWallpaperRemoteId(wallpaper.getWallpaperId());
                mPrefs.setWallpaperPresentationMode(WallpaperPreferences.PRESENTATION_MODE_STATIC);
                mPendingLockWallpaper = bitmap;
                mPrefs.setLockWallpaperAttributions(wallpaper.getAttributions(mAppContext));
                mPrefs.setLockWallpaperRemoteId(wallpaper.getWallpaperId());

                mDestination = WallpaperPersister.DEST_BOTH;
                mCallback = callback;
                mWallpaperPosition = wallpaperPosition;
                mWallpaperInfo = wallpaper;
            }
        });
    }

    @Override
    public boolean setWallpaperInRotation(Bitmap wallpaperBitmap, List<String> attributions,
            int actionLabelRes, int actionIconRes, String actionUrl, String collectionId) {
        if (mFailNextCall) {
            return false;
        }

        mCurrentHomeWallpaper = wallpaperBitmap;
        mCurrentLockWallpaper = wallpaperBitmap;
        mHomeAttributions = attributions;
        mHomeActionUrl = actionUrl;
        return true;
    }

    @Override
    public int setWallpaperBitmapInNextRotation(Bitmap wallpaperBitmap) {
        mCurrentHomeWallpaper = wallpaperBitmap;
        mCurrentLockWallpaper = wallpaperBitmap;
        return 1;
    }

    @Override
    public boolean finalizeWallpaperForNextRotation(List<String> attributions, String actionUrl,
            int actionLabelRes, int actionIconRes, String collectionId, int wallpaperId) {
        mHomeAttributions = attributions;
        mHomeActionUrl = actionUrl;
        return true;
    }

    /** Returns mock system wallpaper bitmap. */
    public Bitmap getCurrentHomeWallpaper() {
        return mCurrentHomeWallpaper;
    }

    /** Returns mock lock screen wallpaper bitmap. */
    public Bitmap getCurrentLockWallpaper() {
        return mCurrentLockWallpaper;
    }

    /** Returns mock home attributions. */
    public List<String> getHomeAttributions() {
        return mHomeAttributions;
    }

    /** Returns the home wallpaper action URL. */
    public String getHomeActionUrl() {
        return mHomeActionUrl;
    }

    /** Returns the Destination a wallpaper was most recently set on. */
    @Destination
    public int getLastDestination() {
        return mDestination;
    }

    /**
     * Sets whether the next "set wallpaper" operation should fail or succeed.
     */
    public void setFailNextCall(Boolean failNextCall) {
        mFailNextCall = failNextCall;
    }

    /**
     * Implemented so synchronous test methods can control the completion of what would otherwise be
     * an asynchronous operation.
     */
    public void finishSettingWallpaper() {
        if (mFailNextCall) {
            mCallback.onError(null /* throwable */);
        } else {
            if (mDestination == DEST_HOME_SCREEN || mDestination == DEST_BOTH) {
                mCurrentHomeWallpaper = mPendingHomeWallpaper;
                mPendingHomeWallpaper = null;
            }
            if (mDestination == DEST_LOCK_SCREEN || mDestination == DEST_BOTH) {
                mCurrentLockWallpaper = mPendingLockWallpaper;
                mPendingLockWallpaper = null;
            }
            mCallback.onSuccess(mWallpaperInfo);
            mWallpaperChangedNotifier.notifyWallpaperChanged();
        }
    }

    @Override
    public void setWallpaperInfoInPreview(WallpaperInfo wallpaperInfo) {
    }

    @Override
    public void onLiveWallpaperSet() {
    }

    /** Returns the last requested wallpaper bitmap scale. */
    public float getScale() {
        return mScale;
    }

    /** Returns the last requested wallpaper crop. */
    public Rect getCropRect() {
        return mCropRect;
    }

    /** Returns the last selected wallpaper position option. */
    @WallpaperPosition
    public int getWallpaperPosition() {
        return mWallpaperPosition;
    }
}
