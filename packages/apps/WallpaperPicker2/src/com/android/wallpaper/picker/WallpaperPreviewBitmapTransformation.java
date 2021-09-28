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
package com.android.wallpaper.picker;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import com.android.wallpaper.util.BitmapProcessor;
import com.android.wallpaper.util.ScreenSizeCalculator;
import com.android.wallpaper.util.WallpaperCropUtils;

import com.bumptech.glide.load.engine.bitmap_recycle.BitmapPool;
import com.bumptech.glide.load.resource.bitmap.BitmapTransformation;

import java.security.MessageDigest;

/**
 * Glide bitmap transformation which emulates the default preview positioning of a wallpaper image.
 */
public class WallpaperPreviewBitmapTransformation extends BitmapTransformation {

    private final Context mContext;
    private Point mScreenSize;
    private boolean mIsRtl;

    public WallpaperPreviewBitmapTransformation(Context appContext, boolean isRtl) {
        WindowManager windowManager = (WindowManager)
                appContext.getSystemService(Context.WINDOW_SERVICE);
        mScreenSize =
                ScreenSizeCalculator.getInstance().getScreenSize(windowManager.getDefaultDisplay());
        mIsRtl = isRtl;
        mContext = appContext;
    }

    @Override
    protected Bitmap transform(@NonNull BitmapPool pool, @NonNull Bitmap toTransform,
            int outWidth, int outHeight) {
        // Transform the thumbnail bitmap to match the default MosaicView positioning.
        float scale = WallpaperCropUtils.calculateMinZoom(
                new Point(toTransform.getWidth(), toTransform.getHeight()),
                mScreenSize);
        Point scaledThumbnailSize = new Point(Math.round(toTransform.getWidth() * scale),
                Math.round(toTransform.getHeight() * scale));
        Point scaledThumbnailToScreenSize = WallpaperCropUtils.calculateCenterPosition(
                scaledThumbnailSize, mScreenSize, false /* alignStart */, mIsRtl);

        Bitmap cropped = Bitmap.createBitmap(toTransform,
                Math.round(scaledThumbnailToScreenSize.x / scale),
                Math.round(scaledThumbnailToScreenSize.y / scale),
                Math.round(mScreenSize.x / scale),
                Math.round(mScreenSize.y / scale));

        return BitmapProcessor.blur(mContext, cropped, cropped.getWidth(), cropped.getHeight());
    }

    @Override
    public void updateDiskCacheKey(MessageDigest messageDigest) {
        messageDigest.update(getId().getBytes());
    }

    /**
     * Returns a unique identifier for this transformation.
     */
    private String getId() {
        return "preview";
    }

}