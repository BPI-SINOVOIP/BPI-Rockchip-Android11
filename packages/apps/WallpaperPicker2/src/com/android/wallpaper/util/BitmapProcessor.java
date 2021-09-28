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
package com.android.wallpaper.util;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.renderscript.Allocation;
import android.renderscript.Element;
import android.renderscript.RenderScript;
import android.renderscript.ScriptIntrinsicBlur;
import android.util.Log;

/**
 * Class with different bitmap processors to apply to bitmaps
 */
public final class BitmapProcessor {

    private static final String TAG = "BitmapProcessor";
    private static final float BLUR_RADIUS = 20f;
    private static final int DOWNSAMPLE = 5;

    private BitmapProcessor() {
    }

    /**
     * Function that blurs the content of a bitmap.
     *
     * @param context   the Application Context
     * @param bitmap    the bitmap we want to blur.
     * @param outWidth  the end width of the blurred bitmap.
     * @param outHeight the end height of the blurred bitmap.
     * @return the blurred bitmap.
     */
    public static Bitmap blur(Context context, Bitmap bitmap, int outWidth,
            int outHeight) {
        RenderScript renderScript = RenderScript.create(context);
        ScriptIntrinsicBlur blur = ScriptIntrinsicBlur.create(renderScript,
                Element.U8_4(renderScript));
        Allocation input = null;
        Allocation output = null;
        Bitmap inBitmap = null;

        try {
            Rect rect = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());
            WallpaperCropUtils.fitToSize(rect, outWidth / DOWNSAMPLE, outHeight / DOWNSAMPLE);

            inBitmap = Bitmap.createScaledBitmap(bitmap, rect.width(), rect.height(),
                    true /* filter */);

            // Render script blurs only support ARGB_8888, we need a conversion if we got a
            // different bitmap config.
            if (inBitmap.getConfig() != Bitmap.Config.ARGB_8888) {
                Bitmap oldIn = inBitmap;
                inBitmap = oldIn.copy(Bitmap.Config.ARGB_8888, false /* isMutable */);
                oldIn.recycle();
            }
            Bitmap outBitmap = Bitmap.createBitmap(inBitmap.getWidth(), inBitmap.getHeight(),
                    Bitmap.Config.ARGB_8888);

            input = Allocation.createFromBitmap(renderScript, inBitmap,
                    Allocation.MipmapControl.MIPMAP_NONE, Allocation.USAGE_GRAPHICS_TEXTURE);
            output = Allocation.createFromBitmap(renderScript, outBitmap);

            blur.setRadius(BLUR_RADIUS);
            blur.setInput(input);
            blur.forEach(output);
            output.copyTo(outBitmap);

            return outBitmap;

        } catch (IllegalArgumentException ex) {
            Log.e(TAG, "error while blurring bitmap", ex);
        } finally {
            if (input != null) {
                input.destroy();
            }

            if (output != null) {
                output.destroy();
            }

            blur.destroy();
            if (inBitmap != null) {
                inBitmap.recycle();
            }
        }

        return null;
    }
}
