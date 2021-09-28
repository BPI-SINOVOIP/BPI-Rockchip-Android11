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

import android.graphics.Bitmap;
import android.graphics.Rect;

import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.Asset.BitmapReceiver;
import com.android.wallpaper.module.BitmapCropper;

/**
 * Test double for BitmapCropper.
 */
public class TestBitmapCropper implements BitmapCropper {

    private boolean mFailNextCall;

    public TestBitmapCropper() {
        mFailNextCall = false;
    }

    @Override
    public void cropAndScaleBitmap(Asset asset, float scale, Rect cropRect,
            Callback callback) {
        if (mFailNextCall) {
            callback.onError(null /* throwable */);
            return;
        }
        // Crop rect in pixels of source image.
        Rect scaledCropRect = new Rect(
                Math.round((float) cropRect.left / scale),
                Math.round((float) cropRect.top / scale),
                Math.round((float) cropRect.right / scale),
                Math.round((float) cropRect.bottom / scale));

        asset.decodeBitmapRegion(scaledCropRect, cropRect.width(), cropRect.height(),
                new BitmapReceiver() {
                    @Override
                    public void onBitmapDecoded(Bitmap bitmap) {
                        callback.onBitmapCropped(bitmap);
                    }
                });
    }
}
