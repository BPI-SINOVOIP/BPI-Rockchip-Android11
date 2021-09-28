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
package com.android.customization.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.customization.model.theme.ThemeBundle;
import com.android.wallpaper.R;

/** A view for displaying style info. */
public class ThemeInfoView extends LinearLayout {
    private static final int WIFI_ICON_PREVIEW_INDEX = 0;
    private static final int SHAPE_PREVIEW_INDEX = 0;

    private TextView mTitle;
    private TextView mFontPreviewTextView;
    private ImageView mIconPreviewImageView;
    private ImageView mAppPreviewImageView;
    private ImageView mShapePreviewImageView;

    public ThemeInfoView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.style_info_title);
        mFontPreviewTextView = findViewById(R.id.font_preview);
        mIconPreviewImageView = findViewById(R.id.qs_preview_icon);
        mAppPreviewImageView = findViewById(R.id.app_preview_icon);
        mShapePreviewImageView = findViewById(R.id.shape_preview_icon);
    }

    /** Populates theme info. */
    public void populateThemeInfo(@NonNull ThemeBundle selectedTheme) {
        ThemeBundle.PreviewInfo previewInfo = selectedTheme.getPreviewInfo();

        if (previewInfo != null) {
            mTitle.setText(getContext().getString(R.string.style_info_description));
            if (previewInfo.headlineFontFamily != null) {
                mTitle.setTypeface(previewInfo.headlineFontFamily);
                mFontPreviewTextView.setTypeface(previewInfo.headlineFontFamily);
            }

            if (previewInfo.icons.get(WIFI_ICON_PREVIEW_INDEX) != null) {
                mIconPreviewImageView.setImageDrawable(
                        previewInfo.icons.get(WIFI_ICON_PREVIEW_INDEX));
            }

            if (previewInfo.shapeAppIcons.get(SHAPE_PREVIEW_INDEX) != null) {
                mAppPreviewImageView.setBackground(
                        previewInfo.shapeAppIcons.get(SHAPE_PREVIEW_INDEX).getDrawableCopy());
            }

            if (previewInfo.shapeDrawable != null) {
                mShapePreviewImageView.setImageDrawable(previewInfo.shapeDrawable);
                mShapePreviewImageView.setImageTintList(
                        ColorStateList.valueOf(previewInfo.resolveAccentColor(getResources())));
            }
        }
    }
}
