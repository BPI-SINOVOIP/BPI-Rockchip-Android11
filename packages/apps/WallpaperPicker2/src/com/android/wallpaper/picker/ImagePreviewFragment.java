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
package com.android.wallpaper.picker;

import static android.view.View.MeasureSpec.EXACTLY;
import static android.view.View.MeasureSpec.makeMeasureSpec;

import static com.android.wallpaper.widget.BottomActionBar.BottomAction.APPLY;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.EDIT;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.INFORMATION;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceControlViewHost;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;

import com.android.wallpaper.R;
import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.CurrentWallpaperAssetVN;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.WallpaperPersister.Destination;
import com.android.wallpaper.module.WallpaperPersister.SetWallpaperCallback;
import com.android.wallpaper.util.ScreenSizeCalculator;
import com.android.wallpaper.util.SizeCalculator;
import com.android.wallpaper.util.WallpaperCropUtils;
import com.android.wallpaper.widget.BottomActionBar;
import com.android.wallpaper.widget.BottomActionBar.AccessibilityCallback;
import com.android.wallpaper.widget.LockScreenPreviewer;
import com.android.wallpaper.widget.WallpaperColorsLoader;
import com.android.wallpaper.widget.WallpaperInfoView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.MemoryCategory;
import com.davemorrissey.labs.subscaleview.ImageSource;
import com.davemorrissey.labs.subscaleview.SubsamplingScaleImageView;
import com.google.android.material.tabs.TabLayout;

import java.util.Locale;

/**
 * Fragment which displays the UI for previewing an individual static wallpaper and its attribution
 * information.
 */
public class ImagePreviewFragment extends PreviewFragment {

    private static final float DEFAULT_WALLPAPER_MAX_ZOOM = 8f;

    private final WallpaperSurfaceCallback mWallpaperSurfaceCallback =
            new WallpaperSurfaceCallback();

    private SubsamplingScaleImageView mFullResImageView;
    private Asset mWallpaperAsset;
    private Point mScreenSize;
    private Point mRawWallpaperSize; // Native size of wallpaper image.
    private ImageView mLowResImageView;
    private TouchForwardingLayout mTouchForwardingLayout;
    private ConstraintLayout mContainer;
    private SurfaceView mWorkspaceSurface;
    private WorkspaceSurfaceHolderCallback mWorkspaceSurfaceCallback;
    private SurfaceView mWallpaperSurface;
    private ViewGroup mLockPreviewContainer;
    private LockScreenPreviewer mLockScreenPreviewer;
    private WallpaperInfoView mWallpaperInfoView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mWallpaperAsset = mWallpaper.getAsset(requireContext().getApplicationContext());
    }

    @Override
    protected int getLayoutResId() {
        return R.layout.fragment_image_preview_v2;
    }

    @Override
    protected int getLoadingIndicatorResId() {
        return R.id.loading_indicator;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);

        Activity activity = requireActivity();
        mScreenSize = ScreenSizeCalculator.getInstance().getScreenSize(
                activity.getWindowManager().getDefaultDisplay());

        // TODO: Consider moving some part of this to the base class when live preview is ready.
        view.findViewById(R.id.low_res_image).setVisibility(View.GONE);
        view.findViewById(R.id.full_res_image).setVisibility(View.GONE);
        mLoadingProgressBar.hide();
        mContainer = view.findViewById(R.id.container);
        mTouchForwardingLayout = mContainer.findViewById(R.id.touch_forwarding_layout);

        // Set aspect ratio on the preview card dynamically.
        ConstraintSet set = new ConstraintSet();
        set.clone(mContainer);
        String ratio = String.format(Locale.US, "%d:%d", mScreenSize.x, mScreenSize.y);
        set.setDimensionRatio(mTouchForwardingLayout.getId(), ratio);
        set.applyTo(mContainer);

        mWorkspaceSurface = mContainer.findViewById(R.id.workspace_surface);
        mWorkspaceSurfaceCallback = new WorkspaceSurfaceHolderCallback(mWorkspaceSurface,
                getContext());
        mWallpaperSurface = mContainer.findViewById(R.id.wallpaper_surface);
        mLockPreviewContainer = mContainer.findViewById(R.id.lock_screen_preview_container);
        mLockScreenPreviewer = new LockScreenPreviewer(getLifecycle(), getActivity(),
                mLockPreviewContainer);

        TabLayout tabs = inflater.inflate(R.layout.full_preview_tabs,
                view.findViewById(R.id.toolbar_tabs_container))
                .findViewById(R.id.full_preview_tabs);
        tabs.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                updateScreenPreview(tab.getPosition() == 0);
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });

        // The TabLayout only contains below tabs, see: full_preview_tabs.xml
        // 0. Home tab
        // 1. Lock tab
        tabs.getTabAt(mViewAsHome ? 0 : 1).select();
        updateScreenPreview(mViewAsHome);

        view.measure(makeMeasureSpec(mScreenSize.x, EXACTLY),
                makeMeasureSpec(mScreenSize.y, EXACTLY));
        ((CardView) mWorkspaceSurface.getParent())
                .setRadius(SizeCalculator.getPreviewCornerRadius(
                        activity, mContainer.getMeasuredWidth()));
        renderImageWallpaper();
        renderWorkspaceSurface();

        // Trim some memory from Glide to make room for the full-size image in this fragment.
        Glide.get(activity).setMemoryCategory(MemoryCategory.LOW);
        setUpLoadingIndicator();
        return view;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        WallpaperColorsLoader.getWallpaperColors(getContext(),
                mWallpaper.getThumbAsset(getContext()),
                mLockScreenPreviewer::setColor);
    }

    @Override
    protected boolean isLoaded() {
        return mFullResImageView != null && mFullResImageView.hasImage();
    }

    @Override
    public void onClickOk() {
        FragmentActivity activity = getActivity();
        if (activity != null) {
            activity.finish();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mLoadingProgressBar != null) {
            mLoadingProgressBar.hide();
        }
        mFullResImageView.recycle();

        mWallpaperSurfaceCallback.cleanUp();
        mWorkspaceSurfaceCallback.cleanUp();
    }

    @Override
    protected void onBottomActionBarReady(BottomActionBar bottomActionBar) {
        super.onBottomActionBarReady(bottomActionBar);

        mWallpaperInfoView = (WallpaperInfoView)
                LayoutInflater.from(getContext()).inflate(
                        R.layout.wallpaper_info_view, /* root= */null);
        mBottomActionBar.attachViewToBottomSheetAndBindAction(mWallpaperInfoView, INFORMATION);
        mBottomActionBar.showActionsOnly(INFORMATION, EDIT, APPLY);
        mBottomActionBar.setActionClickListener(EDIT, v ->
                setEditingEnabled(mBottomActionBar.isActionSelected(EDIT))
        );
        mBottomActionBar.setActionSelectedListener(EDIT, this::setEditingEnabled);
        mBottomActionBar.setActionClickListener(APPLY, this::onSetWallpaperClicked);

        // Update target view's accessibility param since it will be blocked by the bottom sheet
        // when expanded.
        mBottomActionBar.setAccessibilityCallback(new AccessibilityCallback() {
            @Override
            public void onBottomSheetCollapsed() {
                mContainer.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            }

            @Override
            public void onBottomSheetExpanded() {
                mContainer.setImportantForAccessibility(
                        View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
            }
        });

        // Will trigger onActionSelected callback to update the editing state.
        mBottomActionBar.setDefaultSelectedButton(EDIT);
        mBottomActionBar.show();

        mBottomActionBar.disableActions();
        mWallpaperAsset.decodeRawDimensions(getActivity(), dimensions -> {
            // Don't continue loading the wallpaper if the Fragment is detached.
            if (getActivity() == null) {
                return;
            }

            // Return early and show a dialog if dimensions are null (signaling a decoding error).
            if (dimensions == null) {
                showLoadWallpaperErrorDialog();
                return;
            }

            if (mBottomActionBar != null) {
                mBottomActionBar.enableActions();
            }

            mRawWallpaperSize = dimensions;

            setUpExploreIntentAndLabel(ImagePreviewFragment.this::initFullResView);
        });
    }

    /**
     * Initializes MosaicView by initializing tiling, setting a fallback page bitmap, and
     * initializing a zoom-scroll observer and click listener.
     */
    private void initFullResView() {
        // Minimum scale will only be respected under this scale type.
        mFullResImageView.setMinimumScaleType(SubsamplingScaleImageView.SCALE_TYPE_CUSTOM);
        // When we set a minimum scale bigger than the scale with which the full image is shown,
        // disallow user to pan outside the view we show the wallpaper in.
        mFullResImageView.setPanLimit(SubsamplingScaleImageView.PAN_LIMIT_INSIDE);

        // Set a solid black "page bitmap" so MosaicView draws a black background while waiting
        // for the image to load or a transparent one if a thumbnail already loaded.
        Bitmap backgroundBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        int preColor = ContextCompat.getColor(getContext(), R.color.fullscreen_preview_background);
        int color = (mLowResImageView.getDrawable() == null) ? preColor : Color.TRANSPARENT;
        backgroundBitmap.setPixel(0, 0, color);
        mFullResImageView.setImage(ImageSource.bitmap(backgroundBitmap));

        // Then set a fallback "page bitmap" to cover the whole MosaicView, which is an actual
        // (lower res) version of the image to be displayed.
        Point targetPageBitmapSize = new Point(mRawWallpaperSize);
        mWallpaperAsset.decodeBitmap(targetPageBitmapSize.x, targetPageBitmapSize.y,
                pageBitmap -> {
                    // Check that the activity is still around since the decoding task started.
                    if (getActivity() == null) {
                        return;
                    }

                    // Some of these may be null depending on if the Fragment is paused, stopped,
                    // or destroyed.
                    if (mLoadingProgressBar != null) {
                        mLoadingProgressBar.hide();
                    }
                    // The page bitmap may be null if there was a decoding error, so show an
                    // error dialog.
                    if (pageBitmap == null) {
                        showLoadWallpaperErrorDialog();
                        return;
                    }
                    if (mFullResImageView != null) {
                        // Set page bitmap.
                        mFullResImageView.setImage(ImageSource.bitmap(pageBitmap));

                        setDefaultWallpaperZoomAndScroll(
                                mWallpaperAsset instanceof CurrentWallpaperAssetVN);
                        crossFadeInMosaicView();
                    }
                    getActivity().invalidateOptionsMenu();

                    if (mWallpaperInfoView != null && mWallpaper != null) {
                        mWallpaperInfoView.populateWallpaperInfo(mWallpaper, mActionLabel,
                                mExploreIntent, this::onExploreClicked);
                    }
                });
    }

    /**
     * Makes the MosaicView visible with an alpha fade-in animation while fading out the loading
     * indicator.
     */
    private void crossFadeInMosaicView() {
        long shortAnimationDuration = getResources().getInteger(
                android.R.integer.config_shortAnimTime);

        mFullResImageView.setAlpha(0f);
        mFullResImageView.animate()
                .alpha(1f)
                .setDuration(shortAnimationDuration)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Clear the thumbnail bitmap reference to save memory since it's no longer
                        // visible.
                        if (mLowResImageView != null) {
                            mLowResImageView.setImageBitmap(null);
                        }
                    }
                });

        mLoadingProgressBar.animate()
                .alpha(0f)
                .setDuration(shortAnimationDuration)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (mLoadingProgressBar != null) {
                            mLoadingProgressBar.hide();
                        }
                    }
                });
    }

    /**
     * Sets the default wallpaper zoom and scroll position based on a "crop surface" (with extra
     * width to account for parallax) superimposed on the screen. Shows as much of the wallpaper as
     * possible on the crop surface and align screen to crop surface such that the default preview
     * matches what would be seen by the user in the left-most home screen.
     *
     * <p>This method is called once in the Fragment lifecycle after the wallpaper asset has loaded
     * and rendered to the layout.
     *
     * @param offsetToFarLeft {@code true} if we want to offset the visible rectangle to far left of
     *                                    the raw wallpaper; {@code false} otherwise.
     */
    private void setDefaultWallpaperZoomAndScroll(boolean offsetToFarLeft) {
        // Determine minimum zoom to fit maximum visible area of wallpaper on crop surface.
        int cropWidth = mWallpaperSurface.getMeasuredWidth();
        int cropHeight = mWallpaperSurface.getMeasuredHeight();
        Point crop = new Point(cropWidth, cropHeight);
        Rect visibleRawWallpaperRect =
                WallpaperCropUtils.calculateVisibleRect(mRawWallpaperSize, crop);
        if (offsetToFarLeft) {
            visibleRawWallpaperRect.offsetTo(/* newLeft= */ 0, visibleRawWallpaperRect.top);
        }

        final PointF centerPosition = new PointF(visibleRawWallpaperRect.centerX(),
                visibleRawWallpaperRect.centerY());

        Point visibleRawWallpaperSize = new Point(visibleRawWallpaperRect.width(),
                visibleRawWallpaperRect.height());

        final float defaultWallpaperZoom = WallpaperCropUtils.calculateMinZoom(
                visibleRawWallpaperSize, crop);
        final float minWallpaperZoom = defaultWallpaperZoom;


        // Set min wallpaper zoom and max zoom on MosaicView widget.
        mFullResImageView.setMaxScale(Math.max(DEFAULT_WALLPAPER_MAX_ZOOM, defaultWallpaperZoom));
        mFullResImageView.setMinScale(minWallpaperZoom);

        // Set center to composite positioning between scaled wallpaper and screen.
        mFullResImageView.setScaleAndCenter(minWallpaperZoom, centerPosition);
    }

    private Rect calculateCropRect() {
        float wallpaperZoom = mFullResImageView.getScale();
        Context context = requireContext().getApplicationContext();

        Rect visibleFileRect = new Rect();
        mFullResImageView.visibleFileRect(visibleFileRect);

        int cropWidth = mWallpaperSurface.getMeasuredWidth();
        int cropHeight = mWallpaperSurface.getMeasuredHeight();
        int maxCrop = Math.max(cropWidth, cropHeight);
        int minCrop = Math.min(cropWidth, cropHeight);
        Point hostViewSize = new Point(cropWidth, cropHeight);

        Resources res = context.getResources();
        Point cropSurfaceSize = WallpaperCropUtils.calculateCropSurfaceSize(res, maxCrop, minCrop);

        Rect cropRect = WallpaperCropUtils.calculateCropRect(context, hostViewSize,
                cropSurfaceSize, mRawWallpaperSize, visibleFileRect, wallpaperZoom);
        return cropRect;
    }

    @Override
    protected void setCurrentWallpaper(@Destination int destination) {
        mWallpaperSetter.setCurrentWallpaper(getActivity(), mWallpaper, mWallpaperAsset,
                destination, mFullResImageView.getScale(), calculateCropRect(),
                new SetWallpaperCallback() {
                    @Override
                    public void onSuccess(WallpaperInfo wallpaperInfo) {
                        finishActivity(/* success= */ true);
                    }

                    @Override
                    public void onError(@Nullable Throwable throwable) {
                        showSetWallpaperErrorDialog(destination);
                    }
                });
    }

    private void renderWorkspaceSurface() {
        mWorkspaceSurface.setZOrderMediaOverlay(true);
        mWorkspaceSurface.getHolder().addCallback(mWorkspaceSurfaceCallback);
    }

    private void renderImageWallpaper() {
        mWallpaperSurface.getHolder().addCallback(mWallpaperSurfaceCallback);
    }

    private class WallpaperSurfaceCallback implements SurfaceHolder.Callback {

        private Surface mLastSurface;
        private SurfaceControlViewHost mHost;

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            if (mLastSurface != holder.getSurface()) {
                mLastSurface = holder.getSurface();
                if (mFullResImageView != null) {
                    mFullResImageView.recycle();
                }
                Context context = getContext();
                View wallpaperPreviewContainer = LayoutInflater.from(context).inflate(
                        R.layout.fullscreen_wallpaper_preview, null);
                mFullResImageView = wallpaperPreviewContainer.findViewById(R.id.full_res_image);
                mLowResImageView = wallpaperPreviewContainer.findViewById(R.id.low_res_image);
                // Scale the mWallpaperSurface based on system zoom's scale so that the wallpaper is
                // rendered in a larger surface than what preview shows, simulating the behavior of
                // the actual wallpaper surface.
                float scale = WallpaperCropUtils.getSystemWallpaperMaximumScale(context);
                int origWidth = mWallpaperSurface.getWidth();
                int width = (int) (origWidth * scale);
                int origHeight = mWallpaperSurface.getHeight();
                int height = (int) (origHeight * scale);
                int left = (origWidth - width) / 2;
                int top = (origHeight - height) / 2;

                if (WallpaperCropUtils.isRtl(context)) {
                    left *= -1;
                }

                LayoutParams params = mWallpaperSurface.getLayoutParams();
                params.width = width;
                params.height = height;
                mWallpaperSurface.setX(left);
                mWallpaperSurface.setY(top);
                mWallpaperSurface.setLayoutParams(params);
                mWallpaperSurface.requestLayout();

                // Load a low-res placeholder image if there's a thumbnail available from the asset
                // that can be shown to the user more quickly than the full-sized image.
                if (mWallpaperAsset.hasLowResDataSource()) {
                    Activity activity = requireActivity();
                    mWallpaperAsset.loadLowResDrawable(activity, mLowResImageView, Color.BLACK,
                            new WallpaperPreviewBitmapTransformation(
                                    activity.getApplicationContext(), isRtl()));
                }
                wallpaperPreviewContainer.measure(
                        makeMeasureSpec(width, EXACTLY),
                        makeMeasureSpec(height, EXACTLY));
                wallpaperPreviewContainer.layout(0, 0, width, height);
                mTouchForwardingLayout.setTargetView(mFullResImageView);

                cleanUp();
                mHost = new SurfaceControlViewHost(context,
                        context.getDisplay(), mWallpaperSurface.getHostToken());
                mHost.setView(wallpaperPreviewContainer, wallpaperPreviewContainer.getWidth(),
                        wallpaperPreviewContainer.getHeight());
                mWallpaperSurface.setChildSurfacePackage(mHost.getSurfacePackage());
            }
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) { }

        public void cleanUp() {
            if (mHost != null) {
                mHost.release();
                mHost = null;
            }
        }
    };

    private void setEditingEnabled(boolean enabled) {
        mTouchForwardingLayout.setForwardingEnabled(enabled);
    }

    private void updateScreenPreview(boolean isHomeSelected) {
        mWorkspaceSurface.setVisibility(isHomeSelected ? View.VISIBLE : View.INVISIBLE);
        mLockPreviewContainer.setVisibility(isHomeSelected ? View.INVISIBLE : View.VISIBLE);
    }
}
