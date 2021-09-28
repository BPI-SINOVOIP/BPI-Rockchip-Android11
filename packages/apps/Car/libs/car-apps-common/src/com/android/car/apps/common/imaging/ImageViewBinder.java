/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.apps.common.imaging;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.android.car.apps.common.CommonFlags;
import com.android.car.apps.common.R;

/**
 * Binds images to an image view.<p/>
 * While making a new image request (including passing a null {@link ImageBinder.ImageRef} in
 * {@link #setImage}) will cancel the current image request (if any), RecyclerView doesn't
 * always reuse all its views, causing multiple requests to not be canceled. On a slow network,
 * those requests then take time to execute and can make it look like the application has
 * stopped loading images if the user keeps browsing. To prevent that, override:
 * {@link RecyclerView.Adapter#onViewDetachedFromWindow} and call {@link #maybeCancelLoading}
 * {@link RecyclerView.Adapter#onViewAttachedToWindow} and call {@link #maybeRestartLoading}.
 *
 * @param <T> see {@link ImageRef}.
 */
public class ImageViewBinder<T extends ImageBinder.ImageRef> extends ImageBinder<T> {

    @Nullable
    private final ImageView mImageView;
    private final boolean mFlagBitmaps;

    private T mSavedRef;
    private boolean mCancelled;

    /** See {@link ImageViewBinder} and {@link ImageBinder}. */
    public ImageViewBinder(Size maxImageSize, @Nullable ImageView imageView) {
        this(PlaceholderType.FOREGROUND, maxImageSize, imageView, false);
    }

    /**
     * See {@link ImageViewBinder} and {@link ImageBinder}.
     * @param flagBitmaps whether this binder should flag bitmap drawables if flagging is enabled.
     */
    public ImageViewBinder(PlaceholderType type, Size maxImageSize,
            @Nullable ImageView imageView, boolean flagBitmaps) {
        super(type, maxImageSize);
        mImageView = imageView;
        mFlagBitmaps = flagBitmaps;
    }

    @Override
    protected void setDrawable(@Nullable Drawable drawable) {
        if (mImageView != null) {
            mImageView.setImageDrawable(drawable);
            if (mFlagBitmaps) {
                CommonFlags flags = CommonFlags.getInstance(mImageView.getContext());
                if (flags.shouldFlagImproperImageRefs()) {
                    if (drawable instanceof BitmapDrawable) {
                        int tint = mImageView.getContext().getColor(
                                R.color.improper_image_refs_tint_color);
                        mImageView.setColorFilter(tint);
                    } else {
                        mImageView.clearColorFilter();
                    }
                }
            }
        }
    }

    /**
     * Loads a new {@link ImageRef}. The previous request (if any) will be canceled.
     */
    @Override
    public void setImage(Context context, @Nullable T newRef) {
        mSavedRef = newRef;
        mCancelled = false;
        if (mImageView != null) {
            super.setImage(context, newRef);
        }
    }

    /**
     * Restarts the image loading request if {@link #setImage} was called with a valid reference
     * that could not be loaded before {@link #maybeCancelLoading} was called.
     */
    public void maybeRestartLoading(Context context) {
        if (mCancelled) {
            setImage(context, mSavedRef);
        }
    }

    /**
     * Cancels the current loading request (if any) so it doesn't take cycles when the imageView
     * doesn't need the image (like when the view was moved off screen).
     */
    public void maybeCancelLoading(Context context) {
        mCancelled = true;
        if (mImageView != null) {
            super.setImage(context, null); // Call super to keep mSavedRef.
        }
    }

    @Override
    protected void prepareForNewBinding(Context context) {
        mImageView.setImageBitmap(null);
        mImageView.setImageDrawable(null);
        mImageView.clearColorFilter();
        // Call super last to setup the default loading drawable.
        super.prepareForNewBinding(context);
    }

}
