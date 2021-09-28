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
package com.android.wallpaper.picker.individual;

import static com.android.wallpaper.picker.WallpaperPickerDelegate.PREVIEW_LIVE_WALLPAPER_REQUEST_CODE;
import static com.android.wallpaper.picker.WallpaperPickerDelegate.PREVIEW_WALLPAPER_REQUEST_CODE;

import android.app.Activity;
import android.util.Log;
import android.view.View;

import com.android.wallpaper.model.InlinePreviewIntentFactory;
import com.android.wallpaper.model.LiveWallpaperInfo;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.module.WallpaperPersister;
import com.android.wallpaper.picker.PreviewActivity;

/**
 * IndividualHolder subclass for a wallpaper tile in the RecyclerView for which a click should
 * show a full-screen preview of the wallpaper.
 */
class PreviewIndividualHolder extends IndividualHolder implements View.OnClickListener {
    private static final String TAG = "PreviewIndividualHolder";

    private WallpaperPersister mWallpaperPersister;
    private InlinePreviewIntentFactory mPreviewIntentFactory;

    public PreviewIndividualHolder(
            Activity hostActivity, int tileHeightPx, View itemView) {
        super(hostActivity, tileHeightPx, itemView);
        mTileLayout.setOnClickListener(this);

        mWallpaperPersister = InjectorProvider.getInjector().getWallpaperPersister(hostActivity);
        mPreviewIntentFactory = new PreviewActivity.PreviewActivityIntentFactory();
    }

    @Override
    public void onClick(View view) {
        if (mActivity.isFinishing()) {
            Log.w(TAG, "onClick received on VH on finishing Activity");
            return;
        }
        UserEventLogger eventLogger =
                InjectorProvider.getInjector().getUserEventLogger(mActivity);
        eventLogger.logIndividualWallpaperSelected(mWallpaper.getCollectionId(mActivity));

        showPreview(mWallpaper);
    }

    /**
     * Shows the preview activity for the given wallpaper.
     */
    private void showPreview(WallpaperInfo wallpaperInfo) {
        mWallpaperPersister.setWallpaperInfoInPreview(wallpaperInfo);
        wallpaperInfo.showPreview(mActivity, mPreviewIntentFactory,
                wallpaperInfo instanceof LiveWallpaperInfo ? PREVIEW_LIVE_WALLPAPER_REQUEST_CODE
                        : PREVIEW_WALLPAPER_REQUEST_CODE);
    }

}
