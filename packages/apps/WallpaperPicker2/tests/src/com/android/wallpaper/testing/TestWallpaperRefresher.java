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


import android.app.WallpaperManager;
import android.content.Context;

import com.android.wallpaper.compat.BuildCompat;
import com.android.wallpaper.model.WallpaperMetadata;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.WallpaperPreferences;
import com.android.wallpaper.module.WallpaperRefresher;

/**
 * Test implementation of {@link WallpaperRefresher} which simply provides whatever metadata is
 * saved in WallpaperPreferences and the image wallpaper set to {@link WallpaperManager}.
 */
public class TestWallpaperRefresher implements WallpaperRefresher {

    private final Context mAppContext;

    /**
     * @param context The application's context.
     */
    public TestWallpaperRefresher(Context context) {
        mAppContext = context.getApplicationContext();
    }

    @Override
    public void refresh(RefreshListener listener) {

        WallpaperPreferences prefs = InjectorProvider.getInjector().getPreferences(mAppContext);

        if (BuildCompat.isAtLeastN() && prefs.getLockWallpaperId() > 0) {
            listener.onRefreshed(
                    new WallpaperMetadata(
                            prefs.getHomeWallpaperAttributions(),
                            prefs.getHomeWallpaperActionUrl(),
                            prefs.getHomeWallpaperActionLabelRes(),
                            prefs.getHomeWallpaperActionIconRes(),
                            prefs.getHomeWallpaperCollectionId(),
                            prefs.getHomeWallpaperBackingFileName(),
                            null /* wallpaperComponent */),
                    new WallpaperMetadata(
                            prefs.getLockWallpaperAttributions(),
                            prefs.getLockWallpaperActionUrl(),
                            prefs.getLockWallpaperActionLabelRes(),
                            prefs.getLockWallpaperActionIconRes(),
                            prefs.getLockWallpaperCollectionId(),
                            prefs.getLockWallpaperBackingFileName(),
                            null /* wallpaperComponent */),
                    prefs.getWallpaperPresentationMode());
        } else {
            listener.onRefreshed(
                    new WallpaperMetadata(
                            prefs.getHomeWallpaperAttributions(),
                            prefs.getHomeWallpaperActionUrl(),
                            prefs.getHomeWallpaperActionLabelRes(),
                            prefs.getHomeWallpaperActionIconRes(),
                            prefs.getHomeWallpaperCollectionId(),
                            prefs.getHomeWallpaperBackingFileName(),
                            null /* wallpaperComponent */),
                    null,
                    prefs.getWallpaperPresentationMode());
        }
    }
}
