/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.wallpaper.widget;

import android.app.WallpaperColors;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.util.Log;
import android.util.LruCache;
import android.view.Display;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.BitmapCachingAsset;
import com.android.wallpaper.util.ScreenSizeCalculator;

/** A class to load the {@link WallpaperColors} from wallpaper {@link Asset}. */
public class WallpaperColorsLoader {
    private static final String TAG = "WallpaperColorsLoader";

    /** Callback of loading a {@link WallpaperColors}. */
    public interface Callback {
        /** Gets called when a {@link WallpaperColors} is loaded. */
        void onLoaded(@Nullable WallpaperColors colors);
    }

    // The max size should be at least 2 for storing home and lockscreen wallpaper if they are
    // different.
    private static LruCache<Asset, WallpaperColors> sCache = new LruCache<>(/* maxSize= */ 6);

    /** Gets the {@link WallpaperColors} from the wallpaper {@link Asset}. */
    public static void getWallpaperColors(Context context, @NonNull Asset asset,
                                          @NonNull Callback callback) {
        WallpaperColors cached = sCache.get(asset);
        if (cached != null) {
            callback.onLoaded(cached);
            return;
        }

        Display display = context.getSystemService(WindowManager.class).getDefaultDisplay();
        Point screen = ScreenSizeCalculator.getInstance().getScreenSize(display);
        new BitmapCachingAsset(context, asset).decodeBitmap(screen.y / 2, screen.x / 2, bitmap -> {
            if (bitmap != null) {
                boolean shouldRecycle = false;
                if (bitmap.getConfig() == Bitmap.Config.HARDWARE) {
                    bitmap = bitmap.copy(Bitmap.Config.ARGB_8888, false);
                    shouldRecycle = true;
                }
                WallpaperColors colors = WallpaperColors.fromBitmap(bitmap);
                sCache.put(asset, colors);
                callback.onLoaded(colors);
                if (shouldRecycle) {
                    bitmap.recycle();
                }
            } else {
                Log.i(TAG, "Can't get wallpaper colors from a null bitmap, uses null color.");
                callback.onLoaded(null);
            }
        });
    }
}
