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

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.view.ViewOverlay;

import androidx.annotation.Nullable;

/**
 * Translucent Drawable to be rendered on a {@link ViewOverlay} to allow the window background
 * to be seen through it.
 */
public class LiveTileOverlay extends Drawable {

    public static final LiveTileOverlay INSTANCE = new LiveTileOverlay();

    private final Paint mPaint = new Paint();
    private final Rect mBoundsRect = new Rect();

    private RectF mCurrentRect;
    private float mCornerRadius;

    private boolean mIsAttached;

    private Drawable mForegroundDrawable;

    private LiveTileOverlay() {
        mPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
    }

    /**
     * Update the position and radius of the overlay.
     */
    public void update(RectF currentRect, float cornerRadius) {
        invalidateSelf();

        mCurrentRect = currentRect;
        mCornerRadius = cornerRadius;

        mCurrentRect.roundOut(mBoundsRect);
        setBounds(mBoundsRect);
        invalidateSelf();
    }

    @Override
    public void draw(Canvas canvas) {
        if (mCurrentRect != null) {
            canvas.drawRoundRect(mCurrentRect, mCornerRadius, mCornerRadius, mPaint);
            if (mForegroundDrawable != null) {
                mForegroundDrawable.draw(canvas);
            }
        }
    }

    @Override
    public void setAlpha(int i) { }

    @Override
    public void setColorFilter(ColorFilter colorFilter) { }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    /**
     * Attach this drawable to a given {@link ViewOverlay}
     * @return true if the drawable was newly attached, false if it was already attached or it
     *          couldn't be attached
     */
    public boolean attach(ViewOverlay overlay) {
        if (overlay != null && !mIsAttached) {
            overlay.add(this);
            mIsAttached = true;
            return true;
        }

        return false;
    }

    /**
     * Detach this drawable from the given overlay
     * @param overlay
     */
    public void detach(ViewOverlay overlay) {
        if (overlay != null) {
            overlay.remove(this);
            mIsAttached = false;
        }
    }

    /**
     * Set a drawable to be drawn on top of this one.
     */
    public void setForegroundDrawable(@Nullable Drawable foregroundDrawable) {
        mForegroundDrawable = foregroundDrawable;
        invalidateSelf();
    }
}
