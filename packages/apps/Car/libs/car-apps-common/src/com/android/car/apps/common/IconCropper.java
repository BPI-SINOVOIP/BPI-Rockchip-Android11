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

package com.android.car.apps.common;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.core.graphics.PathParser;

/** A helper to crop icons to a shape using a given or the default path. */
public final class IconCropper {
    private static final float ICON_MASK_SIZE = 100.0f;

    @NonNull
    private final Path mIconMask;

    /** Sets up the icon cropper with the given icon mask. */
    public IconCropper(@NonNull Path iconMask) {
        mIconMask = iconMask;
    }

    /**
     * Sets up the icon cropper instance with the default crop mask.
     *
     * The SVG path mask is read from the {@code R.string.config_crop_icon_mask} resource value.
     */
    public IconCropper(@NonNull Context context) {
        this(getDefaultMask(context));
    }

    private static Path getDefaultMask(@NonNull Context context) {
        return PathParser.createPathFromPathData(
                context.getString(R.string.config_crop_icon_mask));
    }

    /** Crops the given drawable according to the current object settings. */
    @NonNull
    public Bitmap crop(@NonNull Drawable source) {
        return crop(BitmapUtils.fromDrawable(source, null));
    }

    /** Crops the given bitmap according to the current object settings. */
    @NonNull
    public Bitmap crop(@NonNull Bitmap icon) {
        int width = icon.getWidth();
        int height = icon.getHeight();

        Bitmap output = Bitmap.createBitmap(width, height, Config.ARGB_8888);
        Canvas canvas = new Canvas(output);

        Paint paint = new Paint();
        paint.setAntiAlias(true);
        // Note: only alpha component of the color set below matters, since we
        // overlay the mask using PorterDuff.Mode.SRC_IN mode (more details here:
        // https://d.android.com/reference/android/graphics/PorterDuff.Mode).
        paint.setColor(Color.WHITE);

        canvas.save();
        canvas.scale(width / ICON_MASK_SIZE, height / ICON_MASK_SIZE);
        canvas.drawPath(mIconMask, paint);
        canvas.restore();

        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        canvas.drawBitmap(icon, 0, 0, paint);

        return output;
    }
}
