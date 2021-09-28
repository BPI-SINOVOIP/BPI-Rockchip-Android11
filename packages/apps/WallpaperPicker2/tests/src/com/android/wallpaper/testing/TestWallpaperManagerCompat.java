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
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import com.android.wallpaper.compat.WallpaperManagerCompat;

import java.io.IOException;
import java.io.InputStream;

/** Test double for {@link WallpaperManagerCompat}. */
public class TestWallpaperManagerCompat extends WallpaperManagerCompat {
    private static final String TAG = "TestWPManagerCompat";

    ParcelFileDescriptor mSystemParcelFd;
    ParcelFileDescriptor mLockParcelFd;
    int mHomeWallpaperId = 0;
    int mLockWallpaperId = 0;

    private boolean mAllowBackup;
    private Context mAppContext;
    private Drawable mTestDrawable;

    public TestWallpaperManagerCompat(Context appContext) {
        mAppContext = appContext;
        mAllowBackup = true;
    }

    @Override
    public int setStream(InputStream stream, Rect visibleCropHint, boolean allowBackup,
            int whichWallpaper) throws IOException {
        mAllowBackup = allowBackup;
        return ++mHomeWallpaperId;
    }

    @Override
    public int setBitmap(Bitmap fullImage, Rect visibleCropHint, boolean allowBackup,
            int whichWallpaper) throws IOException {
        mAllowBackup = allowBackup;
        return ++mLockWallpaperId;
    }

    @Override
    public ParcelFileDescriptor getWallpaperFile(int whichWallpaper) {
        if (whichWallpaper == WallpaperManagerCompat.FLAG_SYSTEM) {
            return mSystemParcelFd;
        } else if (whichWallpaper == WallpaperManagerCompat.FLAG_LOCK) {
            return mLockParcelFd;
        } else {
            // :(
            return null;
        }
    }

    @Override
    public Drawable getDrawable() {
        if (mTestDrawable != null) {
            return mTestDrawable;
        }

        // Retrieve WallpaperManager using Context#getSystemService instead of
        // WallpaperManager#getInstance so it can be mocked out in test.
        WallpaperManager wallpaperManager =
                (WallpaperManager) mAppContext.getSystemService(Context.WALLPAPER_SERVICE);
        return wallpaperManager.getDrawable();
    }

    @Override
    public int getWallpaperId(@WallpaperLocation int whichWallpaper) {
        switch (whichWallpaper) {
            case WallpaperManagerCompat.FLAG_SYSTEM:
                return mHomeWallpaperId;
            case WallpaperManagerCompat.FLAG_LOCK:
                return mLockWallpaperId;
            default:
                throw new IllegalArgumentException(
                        "Wallpaper location must be one of FLAG_SYSTEM or "
                                + "FLAG_LOCK but the value " + whichWallpaper + " was provided.");
        }
    }

    public void setWallpaperFile(int whichWallpaper, ParcelFileDescriptor file) {
        if (whichWallpaper == WallpaperManagerCompat.FLAG_SYSTEM) {
            mSystemParcelFd = file;
        } else if (whichWallpaper == WallpaperManagerCompat.FLAG_LOCK) {
            mLockParcelFd = file;
        } else {
            Log.e(TAG, "Called setWallpaperFile without a valid distinct 'which' argument.");
        }
    }

    public void setWallpaperId(@WallpaperLocation int whichWallpaper, int wallpaperId) {
        switch (whichWallpaper) {
            case WallpaperManagerCompat.FLAG_SYSTEM:
                mHomeWallpaperId = wallpaperId;
                break;
            case WallpaperManagerCompat.FLAG_LOCK:
                mLockWallpaperId = wallpaperId;
                break;
            default:
                throw new IllegalArgumentException(
                        "Wallpaper location must be one of FLAG_SYSTEM or "
                                + "FLAG_LOCK but the value " + whichWallpaper + " was provided.");
        }
    }

    public void setDrawable(Drawable drawable) {
        mTestDrawable = drawable;
    }

    /** Returns whether backup is allowed for the last set wallpaper. */
    public boolean isBackupAllowed() {
        return mAllowBackup;
    }
}
