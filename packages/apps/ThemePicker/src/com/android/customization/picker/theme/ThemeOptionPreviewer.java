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
package com.android.customization.picker.theme;

import static android.view.View.MeasureSpec.EXACTLY;
import static android.view.View.MeasureSpec.makeMeasureSpec;

import android.app.WallpaperColors;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.format.DateFormat;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AnimationUtils;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.Switch;
import android.widget.TextView;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.OnLifecycleEvent;

import com.android.customization.model.theme.ThemeBundle;
import com.android.customization.model.theme.ThemeBundle.PreviewInfo;
import com.android.customization.model.theme.ThemeBundle.PreviewInfo.ShapeAppIcon;
import com.android.wallpaper.R;
import com.android.wallpaper.util.ScreenSizeCalculator;
import com.android.wallpaper.util.TimeUtils;
import com.android.wallpaper.util.TimeUtils.TimeTicker;

import java.util.Calendar;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;

/** A class to load the {@link ThemeBundle} preview to the view. */
class ThemeOptionPreviewer implements LifecycleObserver {
    private static final String DATE_FORMAT = "EEEE, MMM d";

    // Maps which icon from ResourceConstants#ICONS_FOR_PREVIEW.
    private static final int ICON_WIFI = 0;
    private static final int ICON_BLUETOOTH = 1;
    private static final int ICON_FLASHLIGHT = 3;
    private static final int ICON_AUTO_ROTATE = 4;
    private static final int ICON_CELLULAR_SIGNAL = 6;
    private static final int ICON_BATTERY = 7;

    // Icons in the top bar (fake "status bar") with the particular order.
    private static final int [] sTopBarIconToPreviewIcon = new int [] {
            ICON_WIFI, ICON_CELLULAR_SIGNAL, ICON_BATTERY };

    // Ids of app icon shape preview.
    private int[] mShapeAppIconIds = {
            R.id.shape_preview_icon_0, R.id.shape_preview_icon_1,
            R.id.shape_preview_icon_2, R.id.shape_preview_icon_3
    };
    private int[] mShapeIconAppNameIds = {
            R.id.shape_preview_icon_app_name_0, R.id.shape_preview_icon_app_name_1,
            R.id.shape_preview_icon_app_name_2, R.id.shape_preview_icon_app_name_3
    };

    // Ids of color/icons section.
    private int[][] mColorTileIconIds = {
            new int[] { R.id.preview_color_qs_0_icon, ICON_WIFI},
            new int[] { R.id.preview_color_qs_1_icon, ICON_BLUETOOTH},
            new int[] { R.id.preview_color_qs_2_icon, ICON_FLASHLIGHT},
            new int[] { R.id.preview_color_qs_3_icon, ICON_AUTO_ROTATE},
    };
    private int[] mColorTileIds = {
            R.id.preview_color_qs_0_bg, R.id.preview_color_qs_1_bg,
            R.id.preview_color_qs_2_bg, R.id.preview_color_qs_3_bg
    };
    private int[] mColorButtonIds = {
            R.id.preview_check_selected, R.id.preview_radio_selected, R.id.preview_toggle_selected
    };

    private final Context mContext;

    private View mContentView;
    private TextView mStatusBarClock;
    private TextView mSmartSpaceDate;
    private TimeTicker mTicker;

    private boolean mHasPreviewInfoSet;
    private boolean mHasWallpaperColorSet;

    ThemeOptionPreviewer(Lifecycle lifecycle, Context context, ViewGroup previewContainer) {
        lifecycle.addObserver(this);

        mContext = context;
        mContentView = LayoutInflater.from(context).inflate(
                R.layout.theme_preview_content, /* root= */ null);
        mContentView.setVisibility(View.INVISIBLE);
        mStatusBarClock = mContentView.findViewById(R.id.theme_preview_clock);
        mSmartSpaceDate = mContentView.findViewById(R.id.smart_space_date);
        updateTime();
        final float screenAspectRatio =
                ScreenSizeCalculator.getInstance().getScreenAspectRatio(mContext);
        Configuration config = mContext.getResources().getConfiguration();
        final boolean directionLTR = config.getLayoutDirection() == View.LAYOUT_DIRECTION_LTR;
        previewContainer.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View view, int left, int top, int right, int bottom,
                                       int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Calculate the full preview card height and width.
                final int fullPreviewCardHeight = getFullPreviewCardHeight();
                final int fullPreviewCardWidth = (int) (fullPreviewCardHeight / screenAspectRatio);

                // Relayout the content view to match full preview card size.
                mContentView.measure(
                        makeMeasureSpec(fullPreviewCardWidth, EXACTLY),
                        makeMeasureSpec(fullPreviewCardHeight, EXACTLY));
                mContentView.layout(0, 0, fullPreviewCardWidth, fullPreviewCardHeight);

                // Scale the content view from full preview size to the container size. For full
                // preview, the scale value is 1.
                float scale = (float) previewContainer.getMeasuredHeight() / fullPreviewCardHeight;
                mContentView.setScaleX(scale);
                mContentView.setScaleY(scale);
                // The pivot point is centered by default, set to (0, 0).
                mContentView.setPivotX(directionLTR ? 0f : mContentView.getMeasuredWidth());
                mContentView.setPivotY(0f);

                // Ensure there will be only one content view in the container.
                previewContainer.removeAllViews();
                // Finally, add the content view to the container.
                previewContainer.addView(
                        mContentView,
                        mContentView.getMeasuredWidth(),
                        mContentView.getMeasuredHeight());

                previewContainer.removeOnLayoutChangeListener(this);
            }
        });
    }

    /** Loads the Theme option preview into the container view. */
    public void setPreviewInfo(PreviewInfo previewInfo) {
        setHeadlineFont(previewInfo.headlineFontFamily);
        setBodyFont(previewInfo.bodyFontFamily);
        setTopBarIcons(previewInfo.icons);
        setAppIconShape(previewInfo.shapeAppIcons);
        setColorAndIconsSection(previewInfo.icons, previewInfo.shapeDrawable,
                previewInfo.resolveAccentColor(mContext.getResources()));
        setColorAndIconsBoxRadius(previewInfo.bottomSheeetCornerRadius);
        setQsbRadius(previewInfo.bottomSheeetCornerRadius);
        mHasPreviewInfoSet = true;
        showPreviewIfHasAllConfigSet();
    }

    /**
     * Updates the color of widgets in launcher (like top status bar, smart space, and app name
     * text) which will change its content color according to different wallpapers.
     *
     * @param colors the {@link WallpaperColors} of the wallpaper, or {@code null} to use light
     *               color as default
     */
    public void updateColorForLauncherWidgets(@Nullable WallpaperColors colors) {
        boolean useLightTextColor = colors == null
                || (colors.getColorHints() & WallpaperColors.HINT_SUPPORTS_DARK_TEXT) == 0;
        int textColor = mContext.getColor(useLightTextColor
                ? R.color.text_color_light
                : R.color.text_color_dark);
        int textShadowColor = mContext.getColor(useLightTextColor
                ? R.color.smartspace_preview_shadow_color_dark
                : R.color.smartspace_preview_shadow_color_transparent);
        // Update the top status bar clock text color.
        mStatusBarClock.setTextColor(textColor);
        // Update the top status bar icon color.
        ViewGroup iconsContainer = mContentView.findViewById(R.id.theme_preview_top_bar_icons);
        for (int i = 0; i < iconsContainer.getChildCount(); i++) {
            ((ImageView) iconsContainer.getChildAt(i))
                    .setImageTintList(ColorStateList.valueOf(textColor));
        }
        // Update smart space date color.
        mSmartSpaceDate.setTextColor(textColor);
        mSmartSpaceDate.setShadowLayer(
                mContext.getResources().getDimension(
                        R.dimen.smartspace_preview_key_ambient_shadow_blur),
                /* dx = */ 0,
                /* dy = */ 0,
                textShadowColor);

        // Update shape app icon name text color.
        for (int id : mShapeIconAppNameIds) {
            TextView appName = mContentView.findViewById(id);
            appName.setTextColor(textColor);
            appName.setShadowLayer(
                    mContext.getResources().getDimension(
                            R.dimen.preview_theme_app_name_key_ambient_shadow_blur),
                    /* dx = */ 0,
                    /* dy = */ 0,
                    textShadowColor);
        }

        mHasWallpaperColorSet = true;
        showPreviewIfHasAllConfigSet();
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    @MainThread
    public void onResume() {
        mTicker = TimeTicker.registerNewReceiver(mContext, this::updateTime);
        updateTime();
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    @MainThread
    public void onPause() {
        if (mContext != null) {
            mContext.unregisterReceiver(mTicker);
        }
    }

    private void showPreviewIfHasAllConfigSet() {
        if (mHasPreviewInfoSet && mHasWallpaperColorSet
                && mContentView.getVisibility() != View.VISIBLE) {
            mContentView.setAlpha(0f);
            mContentView.setVisibility(View.VISIBLE);
            mContentView.animate().alpha(1f)
                    .setStartDelay(50)
                    .setDuration(200)
                    .setInterpolator(AnimationUtils.loadInterpolator(mContext,
                            android.R.interpolator.fast_out_linear_in))
                    .start();
        }
    }

    private void setHeadlineFont(Typeface headlineFont) {
        mStatusBarClock.setTypeface(headlineFont);
        mSmartSpaceDate.setTypeface(headlineFont);

        // Update font of color/icons section title.
        TextView colorIconsSectionTitle = mContentView.findViewById(R.id.color_icons_section_title);
        colorIconsSectionTitle.setTypeface(headlineFont);
    }

    private void setBodyFont(Typeface bodyFont) {
        // Update font of app names.
        for (int id : mShapeIconAppNameIds) {
            TextView appName = mContentView.findViewById(id);
            appName.setTypeface(bodyFont);
        }
    }

    private void setTopBarIcons(List<Drawable> icons) {
        ViewGroup iconsContainer = mContentView.findViewById(R.id.theme_preview_top_bar_icons);
        for (int i = 0; i < iconsContainer.getChildCount(); i++) {
            int iconIndex = sTopBarIconToPreviewIcon[i];
            if (iconIndex < icons.size()) {
                ((ImageView) iconsContainer.getChildAt(i))
                        .setImageDrawable(icons.get(iconIndex).getConstantState()
                                .newDrawable().mutate());
            } else {
                iconsContainer.getChildAt(i).setVisibility(View.GONE);
            }
        }
    }

    private void setAppIconShape(List<ShapeAppIcon> appIcons) {
        for (int i = 0; i < mShapeAppIconIds.length && i < mShapeIconAppNameIds.length
                && i < appIcons.size(); i++) {
            ShapeAppIcon icon = appIcons.get(i);
            // Set app icon.
            ImageView iconView = mContentView.findViewById(mShapeAppIconIds[i]);
            iconView.setBackground(icon.getDrawableCopy());
            // Set app name.
            TextView appName = mContentView.findViewById(mShapeIconAppNameIds[i]);
            appName.setText(icon.getAppName());
        }
    }

    private void setColorAndIconsSection(List<Drawable> icons, Drawable shapeDrawable,
                                         int accentColor) {
        // Set QS icons and background.
        for (int i = 0; i < mColorTileIconIds.length && i < icons.size(); i++) {
            Drawable icon = icons.get(mColorTileIconIds[i][1]).getConstantState()
                    .newDrawable().mutate();
            Drawable bgShape = shapeDrawable.getConstantState().newDrawable();
            bgShape.setTint(accentColor);

            ImageView bg = mContentView.findViewById(mColorTileIds[i]);
            bg.setImageDrawable(bgShape);
            ImageView fg = mContentView.findViewById(mColorTileIconIds[i][0]);
            fg.setImageDrawable(icon);
        }

        // Set color for Buttons (CheckBox, RadioButton, and Switch).
        ColorStateList tintList = getColorStateList(accentColor);
        for (int mColorButtonId : mColorButtonIds) {
            CompoundButton button = mContentView.findViewById(mColorButtonId);
            button.setButtonTintList(tintList);
            if (button instanceof Switch) {
                ((Switch) button).setThumbTintList(tintList);
                ((Switch) button).setTrackTintList(tintList);
            }
        }
    }

    private void setColorAndIconsBoxRadius(int cornerRadius) {
        ((CardView) mContentView.findViewById(R.id.color_icons_section)).setRadius(cornerRadius);
    }

    private void setQsbRadius(int cornerRadius) {
        View qsb = mContentView.findViewById(R.id.theme_qsb);
        if (qsb != null && qsb.getVisibility() == View.VISIBLE) {
            if (qsb.getBackground() instanceof GradientDrawable) {
                GradientDrawable bg = (GradientDrawable) qsb.getBackground();
                float radius = useRoundedQSB(cornerRadius)
                        ? (float) qsb.getLayoutParams().height / 2 : cornerRadius;
                bg.setCornerRadii(new float[]{
                        radius, radius, radius, radius,
                        radius, radius, radius, radius});
            }
        }
    }

    private void updateTime() {
        Calendar calendar = Calendar.getInstance(TimeZone.getDefault());
        if (mStatusBarClock != null) {
            mStatusBarClock.setText(TimeUtils.getFormattedTime(mContext, calendar));
        }
        if (mSmartSpaceDate != null) {
            String datePattern =
                    DateFormat.getBestDateTimePattern(Locale.getDefault(), DATE_FORMAT);
            mSmartSpaceDate.setText(DateFormat.format(datePattern, calendar));
        }
    }

    private boolean useRoundedQSB(int cornerRadius) {
        return cornerRadius >= mContext.getResources().getDimensionPixelSize(
                R.dimen.roundCornerThreshold);
    }

    private ColorStateList getColorStateList(int accentColor) {
        int controlGreyColor = mContext.getColor(R.color.control_grey);
        return new ColorStateList(
                new int[][]{
                        new int[]{android.R.attr.state_selected},
                        new int[]{android.R.attr.state_checked},
                        new int[]{-android.R.attr.state_enabled},
                },
                new int[] {
                        accentColor,
                        accentColor,
                        controlGreyColor
                }
        );
    }

    /**
     * Gets the screen height which does not include the system status bar and bottom navigation
     * bar.
     */
    private int getDisplayHeight() {
        final DisplayMetrics dm = mContext.getResources().getDisplayMetrics();
        return dm.heightPixels;
    }

    // The height of top tool bar (R.layout.section_header).
    private int getTopToolBarHeight() {
        final TypedValue typedValue = new TypedValue();
        return mContext.getTheme().resolveAttribute(
                android.R.attr.actionBarSize, typedValue, true)
                ? TypedValue.complexToDimensionPixelSize(
                        typedValue.data, mContext.getResources().getDisplayMetrics())
                : 0;
    }

    private int getFullPreviewCardHeight() {
        final Resources res = mContext.getResources();
        return getDisplayHeight()
                - getTopToolBarHeight()
                - res.getDimensionPixelSize(R.dimen.bottom_navbar_height)
                - res.getDimensionPixelSize(R.dimen.full_preview_page_default_padding_top)
                - res.getDimensionPixelSize(R.dimen.full_preview_page_default_padding_bottom);
    }
}
