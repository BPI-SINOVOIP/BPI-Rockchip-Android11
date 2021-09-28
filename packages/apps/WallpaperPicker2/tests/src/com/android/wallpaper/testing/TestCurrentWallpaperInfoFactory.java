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

import android.content.Context;

import com.android.wallpaper.compat.BuildCompat;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.CurrentWallpaperInfoFactory;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.WallpaperRefresher;

import java.util.List;

/**
 * Test double of {@link CurrentWallpaperInfoFactory}.
 */
public class TestCurrentWallpaperInfoFactory implements CurrentWallpaperInfoFactory {

    private WallpaperRefresher mRefresher;

    public TestCurrentWallpaperInfoFactory(Context context) {
        mRefresher = InjectorProvider.getInjector().getWallpaperRefresher(
                context.getApplicationContext());
    }

    @Override
    public void createCurrentWallpaperInfos(final WallpaperInfoCallback callback,
            boolean forceRefresh) {
        mRefresher.refresh((homeWallpaperMetadata, lockWallpaperMetadata, presentationMode) -> {

            WallpaperInfo homeWallpaper = createTestWallpaperInfo(
                    homeWallpaperMetadata.getAttributions(),
                    homeWallpaperMetadata.getActionUrl(),
                    homeWallpaperMetadata.getCollectionId());

            WallpaperInfo lockWallpaper = null;
            if (lockWallpaperMetadata != null && BuildCompat.isAtLeastN()) {
                lockWallpaper = createTestWallpaperInfo(
                        lockWallpaperMetadata.getAttributions(),
                        lockWallpaperMetadata.getActionUrl(),
                        lockWallpaperMetadata.getCollectionId());
            }

            callback.onWallpaperInfoCreated(homeWallpaper, lockWallpaper, presentationMode);
        });
    }

    private static WallpaperInfo createTestWallpaperInfo(List<String> attributions,
            String actionUrl, String collectionId) {
        TestWallpaperInfo wallpaper = new TestWallpaperInfo(TestWallpaperInfo.COLOR_BLACK);
        wallpaper.setAttributions(attributions);
        wallpaper.setActionUrl(actionUrl);
        wallpaper.setCollectionId(collectionId);
        return wallpaper;
    }
}
