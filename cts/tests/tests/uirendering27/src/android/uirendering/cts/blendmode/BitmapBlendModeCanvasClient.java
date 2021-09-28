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
 * limitations under the License
 */

package android.uirendering.cts.blendmode;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.uirendering.cts.testinfrastructure.CanvasClient;

/**
 * CanvasClient used to draw bitmaps with the clear and dstover blend modes to verify
 * that backward compatible behavior is respected on API levels 27 and older where clear is treated
 * as dst out for drawing bitmaps. However, on newer API levels, the clear blend mode is
 * properly respected
 */
public class BitmapBlendModeCanvasClient implements CanvasClient {

    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    /**
     * Color used to render the circle in the mask bitmap
     */
    private final int mBitmapColor;

    /**
     * Color used to draw tint the area overlapping the content in the mask
     */
    private final int mMaskColor;

    /**
     * PorterDuff mode used to mask out the content in the bitmap mask
     */
    private final PorterDuff.Mode mBitmapMode;

    /**
     * PorterDuff moe used to render over the content in the bitmap mask
     */
    private final PorterDuff.Mode mMaskMode;

    public BitmapBlendModeCanvasClient(PorterDuff.Mode bitmapMode, PorterDuff.Mode maskMode,
            int bitmapColor, int maskColor) {
        mBitmapMode = bitmapMode;
        mMaskMode = maskMode;
        mBitmapColor = bitmapColor;
        mMaskColor = maskColor;
    }

    @Override
    public void draw(Canvas canvas, int width, int height) {
        Bitmap bitmap = createBitmap(width, height);

        mPaint.setXfermode(new PorterDuffXfermode(mBitmapMode));
        canvas.drawBitmap(bitmap, 0.0f, 0.0f, mPaint);

        mPaint.setXfermode(new PorterDuffXfermode(mMaskMode));
        mPaint.setColor(mMaskColor);
        canvas.drawRect(0, 0, width, height, mPaint);
    }

    private Bitmap createBitmap(int width, int height) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(mBitmapColor);
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawCircle(
                width / 2.0f,
                height / 2.0f,
                Math.min(width, height) / 2.0f,
                paint);
        return bitmap;
    }
}