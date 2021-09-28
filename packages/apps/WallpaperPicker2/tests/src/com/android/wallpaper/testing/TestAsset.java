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
import android.graphics.Bitmap.Config;
import android.graphics.Point;
import android.graphics.Rect;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.android.wallpaper.asset.Asset;


/**
 * Test implementation of Asset which blocks on Bitmap decoding operations.
 */
public final class TestAsset extends Asset {

    private Bitmap mBitmap;
    private boolean mIsCorrupt;

    /**
     * Constructs an asset underpinned by a 1x1 bitmap uniquely identifiable by the given pixel
     * color.
     *
     * @param pixelColor Color of the asset's single pixel.
     * @param isCorrupt  Whether or not the asset is corrupt and fails to validly decode bitmaps and
     *                   dimensions.
     */
    public TestAsset(int pixelColor, boolean isCorrupt) {
        mIsCorrupt = isCorrupt;

        if (!mIsCorrupt) {
            mBitmap = Bitmap.createBitmap(1, 1, Config.ARGB_8888);
            mBitmap.setPixel(0, 0, pixelColor);
        } else {
            mBitmap = null;
        }
    }

    @Override
    public void decodeBitmap(int targetWidth, int targetHeight, BitmapReceiver receiver) {
        receiver.onBitmapDecoded(mBitmap);
    }

    @Override
    public void decodeBitmapRegion(Rect unused, int targetWidth, int targetHeight,
            BitmapReceiver receiver) {
        receiver.onBitmapDecoded(mBitmap);
    }

    @Override
    public void decodeRawDimensions(Activity unused, DimensionsReceiver receiver) {
        receiver.onDimensionsDecoded(mIsCorrupt ? null : new Point(1, 1));
    }

    @Override
    public boolean supportsTiling() {
        return false;
    }

    @Override
    public void loadDrawableWithTransition(
            Context context,
            ImageView imageView,
            int transitionDurationMillis,
            @Nullable DrawableLoadedListener drawableLoadedListener,
            int placeholderColor) {
        if (drawableLoadedListener != null) {
            drawableLoadedListener.onDrawableLoaded();
        }
    }

    /** Returns the bitmap synchronously. Convenience method for tests. */
    public Bitmap getBitmap() {
        return mBitmap;
    }

    public void setBitmap(Bitmap bitmap) {
        mBitmap = bitmap;
    }
}
