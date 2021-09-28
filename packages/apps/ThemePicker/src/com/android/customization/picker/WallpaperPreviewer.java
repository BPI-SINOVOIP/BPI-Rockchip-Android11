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
package com.android.customization.picker;

import android.app.Activity;
import android.app.WallpaperColors;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.RectF;
import android.service.wallpaper.WallpaperService;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.OnLifecycleEvent;

import com.android.wallpaper.R;
import com.android.wallpaper.model.LiveWallpaperInfo;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.util.ScreenSizeCalculator;
import com.android.wallpaper.util.SizeCalculator;
import com.android.wallpaper.util.WallpaperConnection;
import com.android.wallpaper.util.WallpaperConnection.WallpaperConnectionListener;
import com.android.wallpaper.util.WallpaperSurfaceCallback;
import com.android.wallpaper.widget.LiveTileOverlay;
import com.android.wallpaper.widget.WallpaperColorsLoader;

/** A class to load the wallpaper to the view. */
public class WallpaperPreviewer implements LifecycleObserver {

    private final Rect mPreviewLocalRect = new Rect();
    private final Rect mPreviewGlobalRect = new Rect();
    private final int[] mLivePreviewLocation = new int[2];

    private final Activity mActivity;
    private final ImageView mHomePreview;
    private final SurfaceView mWallpaperSurface;

    private WallpaperSurfaceCallback mWallpaperSurfaceCallback;
    private WallpaperInfo mWallpaper;
    private WallpaperConnection mWallpaperConnection;
    @Nullable private WallpaperColorsListener mWallpaperColorsListener;

    /** Interface for getting {@link WallpaperColors} from wallpaper. */
    public interface WallpaperColorsListener {
        /** Gets called when wallpaper color is available or updated. */
        void onWallpaperColorsChanged(WallpaperColors colors);
    }

    public WallpaperPreviewer(Lifecycle lifecycle, Activity activity, ImageView homePreview,
                              SurfaceView wallpaperSurface) {
        lifecycle.addObserver(this);

        mActivity = activity;
        mHomePreview = homePreview;
        mWallpaperSurface = wallpaperSurface;
        mWallpaperSurfaceCallback = new WallpaperSurfaceCallback(activity, mHomePreview,
                mWallpaperSurface, this::setUpWallpaperPreview);
        mWallpaperSurface.setZOrderMediaOverlay(false);
        mWallpaperSurface.getHolder().addCallback(mWallpaperSurfaceCallback);

        View rootView = homePreview.getRootView();
        rootView.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                                       int oldLeft, int oldTop, int oldRight, int oldBottom) {
                updatePreviewCardRadius();
                rootView.removeOnLayoutChangeListener(this);
            }
        });
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    @MainThread
    public void onResume() {
        if (mWallpaperConnection != null) {
            mWallpaperConnection.setVisibility(true);
        }
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    @MainThread
    public void onPause() {
        if (mWallpaperConnection != null) {
            mWallpaperConnection.setVisibility(false);
        }
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    @MainThread
    public void onStop() {
        if (mWallpaperConnection != null) {
            mWallpaperConnection.disconnect();
            mWallpaperConnection = null;
        }
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    @MainThread
    public void onDestroy() {
        LiveTileOverlay.INSTANCE.detach(mHomePreview.getOverlay());
        if (mWallpaperConnection != null) {
            mWallpaperConnection.disconnect();
            mWallpaperConnection = null;
        }

        mWallpaperSurfaceCallback.cleanUp();
        mWallpaperSurface.getHolder().removeCallback(mWallpaperSurfaceCallback);
        Surface surface = mWallpaperSurface.getHolder().getSurface();
        if (surface != null) {
            surface.release();
        }
    }

    /**
     * Sets a wallpaper to be shown on preview screen.
     *
     * @param wallpaperInfo the wallpaper to preview
     * @param listener the listener for getting the wallpaper color of {@param wallpaperInfo}
     */
    public void setWallpaper(WallpaperInfo wallpaperInfo,
                             @Nullable WallpaperColorsListener listener) {
        mWallpaper = wallpaperInfo;
        mWallpaperColorsListener = listener;
        setUpWallpaperPreview();
    }

    private void setUpWallpaperPreview() {
        ImageView homeImageWallpaper = mWallpaperSurfaceCallback.getHomeImageWallpaper();
        if (mWallpaper != null && homeImageWallpaper != null) {
            homeImageWallpaper.post(() -> {
                boolean renderInImageWallpaperSurface = !(mWallpaper instanceof LiveWallpaperInfo);
                mWallpaper.getThumbAsset(mActivity.getApplicationContext())
                        .loadPreviewImage(mActivity,
                                renderInImageWallpaperSurface ? homeImageWallpaper : mHomePreview,
                                mActivity.getResources().getColor(R.color.secondary_color));
                LiveTileOverlay.INSTANCE.detach(mHomePreview.getOverlay());
                if (mWallpaper instanceof LiveWallpaperInfo) {
                    mWallpaper.getThumbAsset(mActivity.getApplicationContext())
                            .loadPreviewImage(
                                    mActivity,
                                    homeImageWallpaper,
                                    mActivity.getColor(R.color.secondary_color));
                    setUpLiveWallpaperPreview(mWallpaper);
                } else {
                    // Ensure live wallpaper connection is disconnected.
                    if (mWallpaperConnection != null) {
                        mWallpaperConnection.disconnect();
                        mWallpaperConnection = null;
                    }

                    // Load wallpaper color for static wallpaper.
                    if (mWallpaperColorsListener != null) {
                        WallpaperColorsLoader.getWallpaperColors(
                                mActivity,
                                mWallpaper.getThumbAsset(mActivity),
                                mWallpaperColorsListener::onWallpaperColorsChanged);
                    }
                }
            });
        }
    }

    private void setUpLiveWallpaperPreview(WallpaperInfo homeWallpaper) {
        if (mActivity == null || mActivity.isFinishing()) {
            return;
        }

        if (mWallpaperConnection != null) {
            mWallpaperConnection.disconnect();
        }
        if (WallpaperConnection.isPreviewAvailable()) {
            mHomePreview.getLocationOnScreen(mLivePreviewLocation);
            mPreviewGlobalRect.set(0, 0, mHomePreview.getMeasuredWidth(),
                    mHomePreview.getMeasuredHeight());
            mPreviewLocalRect.set(mPreviewGlobalRect);
            mPreviewGlobalRect.offset(mLivePreviewLocation[0], mLivePreviewLocation[1]);

            mWallpaperConnection = new WallpaperConnection(
                    getWallpaperIntent(homeWallpaper.getWallpaperComponent()), mActivity,
                    new WallpaperConnectionListener() {
                        @Override
                        public void onWallpaperColorsChanged(WallpaperColors colors,
                                int displayId) {
                            if (mWallpaperColorsListener != null) {
                                mWallpaperColorsListener.onWallpaperColorsChanged(colors);
                            }
                        }
                    }, mPreviewGlobalRect);

            LiveTileOverlay.INSTANCE.update(new RectF(mPreviewLocalRect),
                    ((CardView) mHomePreview.getParent()).getRadius());

            mWallpaperConnection.setVisibility(true);
            mHomePreview.post(() -> {
                if (mWallpaperConnection != null && !mWallpaperConnection.connect()) {
                    mWallpaperConnection = null;
                    LiveTileOverlay.INSTANCE.detach(mHomePreview.getOverlay());
                }
            });
        } else {
            // Load wallpaper color from the thumbnail.
            if (mWallpaperColorsListener != null) {
                WallpaperColorsLoader.getWallpaperColors(
                        mActivity,
                        mWallpaper.getThumbAsset(mActivity),
                        mWallpaperColorsListener::onWallpaperColorsChanged);
            }
        }
    }

    /** Updates the preview card view corner radius to match the device corner radius. */
    private void updatePreviewCardRadius() {
        final float screenAspectRatio =
                ScreenSizeCalculator.getInstance().getScreenAspectRatio(mActivity);
        CardView cardView = (CardView) mHomePreview.getParent();
        final int cardWidth = (int) (cardView.getMeasuredHeight() / screenAspectRatio);
        ViewGroup.LayoutParams layoutParams = cardView.getLayoutParams();
        layoutParams.width = cardWidth;
        cardView.setLayoutParams(layoutParams);
        cardView.setRadius(SizeCalculator.getPreviewCornerRadius(mActivity, cardWidth));
    }

    private static Intent getWallpaperIntent(android.app.WallpaperInfo info) {
        return new Intent(WallpaperService.SERVICE_INTERFACE)
                .setClassName(info.getPackageName(), info.getServiceName());
    }
}
